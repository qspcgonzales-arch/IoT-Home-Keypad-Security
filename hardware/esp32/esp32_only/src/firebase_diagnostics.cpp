#include "firebase_diagnostics.h"

#ifndef ESP32_CLOUD_DIAGNOSTICS
#define ESP32_CLOUD_DIAGNOSTICS 0
#endif

#if ESP32_CLOUD_DIAGNOSTICS

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <sys/time.h>

#include <algorithm>
#include <cstring>

#include "cloud_config.h"

#if SERVO_TEST_BUILD
#error "Cloud diagnostics must compile without servo actuation."
#endif

namespace {
constexpr uint32_t kHeartbeatIntervalMs = 5000;
constexpr uint32_t kCommandPollIntervalMs = 1000;
constexpr uint32_t kMaximumCommandLifetimeMs = 60000;
constexpr uint32_t kMinimumTokenLifetimeSeconds = 60;
constexpr uint32_t kTokenRefreshMarginSeconds = 300;
constexpr uint8_t kMaximumAuditQueueSize = 8;
constexpr uint64_t kMinimumValidEpochMs = 1700000000000ULL;
constexpr uint64_t kMaximumValidEpochMs = 2051222400000ULL;

bool httpSuccess(int code) { return code >= 200 && code < 300; }
}  // namespace

class FirebaseDiagnostics::State {
 public:
  Preferences preferences;
  String idToken;
  String refreshToken;
  uint32_t wifiRetryAt = 0;
  uint32_t wifiRetryDelay = 2000;
  uint32_t authRetryAt = 0;
  uint32_t authRetryDelay = 5000;
  uint32_t tokenRefreshAt = 0;
  uint32_t registrationCheckAt = 0;
  uint32_t heartbeatAt = 0;
  uint32_t commandPollAt = 0;
  uint32_t auditRetryAt = 0;
  uint32_t auditRetryDelay = 1000;
  bool preferencesReady = false;
  bool configured = false;
  bool authenticated = false;
  bool registered = false;
  bool lastWifiConnected = false;
};

FirebaseDiagnostics::~FirebaseDiagnostics() { delete _state; }

void FirebaseDiagnostics::begin() {
  if (_state == nullptr) _state = new State();
  _state->preferencesReady = _state->preferences.begin("esp32-cloud", false);
  _state->configured = isConfigured() && _state->preferencesReady;
  if (!_state->preferencesReady) {
    Serial.println("Cloud diagnostics disabled: local storage unavailable.");
    return;
  }
  if (!isConfigured()) {
    Serial.println("Cloud diagnostics disabled: staging configuration is incomplete.");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.println("ESP32 cloud diagnostics configured for the explicit staging project.");
}

bool FirebaseDiagnostics::isConfigured() const {
  const String projectId = FIREBASE_PROJECT_ID;
  const String databaseEndpoint = FIREBASE_DATABASE_URL;
  const bool databaseMatchesProject =
      databaseEndpoint.startsWith("https://" + projectId +
                                  "-default-rtdb.") ||
      databaseEndpoint.startsWith("https://" + projectId +
                                  ".firebaseio.com");
  const bool firebaseDatabaseHost =
      databaseEndpoint.endsWith(".firebasedatabase.app") ||
      databaseEndpoint.endsWith(".firebaseio.com");
  return WIFI_SSID[0] && WIFI_PASSWORD[0] &&
         FIREBASE_PROJECT_ID[0] && FIREBASE_DATABASE_URL[0] &&
         FIREBASE_API_KEY[0] && FIREBASE_DEVICE_EMAIL[0] &&
         FIREBASE_DEVICE_PASSWORD[0] && FIREBASE_DEVICE_UID[0] &&
         FIREBASE_ADMIN_UID[0] && FIREBASE_DEVICE_ID[0] &&
         strcmp(FIREBASE_PROJECT_ID, "iothomekeypadsecurity-staging") == 0 &&
         strcmp(
             FIREBASE_DATABASE_URL,
             "https://iothomekeypadsecurity-staging-default-rtdb."
             "asia-southeast1.firebasedatabase.app") == 0 &&
         strcmp(FIREBASE_DEVICE_UID, FIREBASE_ADMIN_UID) != 0 &&
         databaseMatchesProject && firebaseDatabaseHost &&
         validIdentifier(FIREBASE_DEVICE_ID, 64) &&
         strcmp(FIREBASE_DEVICE_ID, "esp32-frontdoor-1") == 0;
}

uint64_t FirebaseDiagnostics::nowEpochMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<uint64_t>(tv.tv_sec) * 1000ULL +
         static_cast<uint64_t>(tv.tv_usec / 1000);
}

bool FirebaseDiagnostics::clockIsValid() {
  const uint64_t now = nowEpochMs();
  return now >= kMinimumValidEpochMs && now <= kMaximumValidEpochMs;
}

bool FirebaseDiagnostics::deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

bool FirebaseDiagnostics::validIdentifier(const String &value,
                                          size_t maxLength) {
  if (value.isEmpty() || value.length() > maxLength) return false;
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') || c == '_' || c == '-')) {
      return false;
    }
  }
  return true;
}

String FirebaseDiagnostics::encodeUrl(const String &value) const {
  static const char hex[] = "0123456789ABCDEF";
  String encoded;
  encoded.reserve(value.length() * 3);
  for (size_t i = 0; i < value.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(value[i]);
    const bool unreserved =
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~';
    if (unreserved) {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%';
      encoded += hex[c >> 4];
      encoded += hex[c & 0x0F];
    }
  }
  return encoded;
}

String FirebaseDiagnostics::authUrl(const String &endpoint) const {
  return "https://identitytoolkit.googleapis.com/v1/" + endpoint +
         "?key=" + encodeUrl(FIREBASE_API_KEY);
}

String FirebaseDiagnostics::tokenUrl() const {
  return "https://securetoken.googleapis.com/v1/token?key=" +
         encodeUrl(FIREBASE_API_KEY);
}

String FirebaseDiagnostics::databaseUrl(const String &path,
                                        const String &query) const {
  String base = FIREBASE_DATABASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  String url = base + "/" + path + ".json";
  bool hasQuery = false;
  if (!query.isEmpty()) {
    url += "?" + query;
    hasQuery = true;
  }
  if (_state != nullptr && !_state->idToken.isEmpty()) {
    url += hasQuery ? "&" : "?";
    url += "auth=" + encodeUrl(_state->idToken);
  }
  return url;
}

int FirebaseDiagnostics::sendRequest(const String &method, const String &url,
                                     const String &payload, String &response,
                                     const char *contentType) {
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  WiFiClientSecure client;
  client.setHandshakeTimeout(5);
  client.useBuiltinCACertBundle();
  if (!http.begin(client, url)) return HTTPC_ERROR_CONNECTION_REFUSED;
  http.addHeader("Accept", "application/json");
  if (contentType != nullptr) http.addHeader("Content-Type", contentType);
  const int code = http.sendRequest(method.c_str(), payload);
  if (code > 0) response = http.getString();
  http.end();
  return code;
}

void FirebaseDiagnostics::maintainWifi() {
  if (_state == nullptr || WiFi.status() == WL_CONNECTED) return;
  if (!deadlineReached(millis(), _state->wifiRetryAt)) return;
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  _state->wifiRetryAt = millis() + _state->wifiRetryDelay;
  _state->wifiRetryDelay =
      std::min<uint32_t>(_state->wifiRetryDelay * 2, 60000);
}

void FirebaseDiagnostics::scheduleAuthRetry() {
  if (_state == nullptr) return;
  _state->authRetryAt = millis() + _state->authRetryDelay;
  _state->authRetryDelay =
      std::min<uint32_t>(_state->authRetryDelay * 2, 60000);
}

void FirebaseDiagnostics::scheduleDatabaseRetry() {
  if (_state == nullptr) return;
  _state->auditRetryAt = millis() + _state->auditRetryDelay;
  _state->auditRetryDelay =
      std::min<uint32_t>(_state->auditRetryDelay * 2, 30000);
}

bool FirebaseDiagnostics::signIn() {
  JsonDocument request;
  request["email"] = FIREBASE_DEVICE_EMAIL;
  request["password"] = FIREBASE_DEVICE_PASSWORD;
  request["returnSecureToken"] = true;
  String payload;
  serializeJson(request, payload);
  String response;
  const int code = sendRequest(
      "POST", authUrl("accounts:signInWithPassword"), payload, response);
  if (!httpSuccess(code)) {
    Serial.printf("Staging device sign-in failed (HTTP %d).\n", code);
    _state->authenticated = false;
    scheduleAuthRetry();
    return false;
  }
  JsonDocument result;
  if (deserializeJson(result, response) ||
      !result["idToken"].is<const char *>() ||
      !result["refreshToken"].is<const char *>() ||
      !result["localId"].is<const char *>() ||
      !result["expiresIn"].is<const char *>()) {
    Serial.println("Staging device sign-in returned an invalid response.");
    _state->authenticated = false;
    scheduleAuthRetry();
    return false;
  }
  if (result["localId"].as<String>() != FIREBASE_DEVICE_UID ||
      result["localId"].as<String>() == FIREBASE_ADMIN_UID) {
    Serial.println("Staging sign-in UID does not match the dedicated device UID.");
    _state->authenticated = false;
    scheduleAuthRetry();
    return false;
  }
  _state->idToken = result["idToken"].as<String>();
  _state->refreshToken = result["refreshToken"].as<String>();
  const uint32_t lifetime =
      strtoul(result["expiresIn"].as<const char *>(), nullptr, 10);
  const uint32_t refreshIn =
      std::max<uint32_t>(kMinimumTokenLifetimeSeconds,
                         lifetime > kTokenRefreshMarginSeconds
                             ? lifetime - kTokenRefreshMarginSeconds
                             : kMinimumTokenLifetimeSeconds);
  _state->tokenRefreshAt = millis() + refreshIn * 1000UL;
  _state->authRetryDelay = 5000;
  _state->authenticated = true;
  _state->registered = false;
  Serial.println("Dedicated staging device identity authenticated.");
  return true;
}

bool FirebaseDiagnostics::refreshAuth() {
  if (_state->refreshToken.isEmpty()) {
    _state->authenticated = false;
    return signIn();
  }
  const String payload =
      "grant_type=refresh_token&refresh_token=" +
      encodeUrl(_state->refreshToken);
  String response;
  const int code = sendRequest("POST", tokenUrl(), payload, response,
                               "application/x-www-form-urlencoded");
  if (!httpSuccess(code)) {
    Serial.printf("Staging device token refresh failed (HTTP %d).\n", code);
    if (code == 400 || code == 401) {
      _state->idToken = "";
      _state->refreshToken = "";
      _state->authenticated = false;
    }
    scheduleAuthRetry();
    _state->tokenRefreshAt = _state->authRetryAt;
    return false;
  }
  JsonDocument result;
  if (deserializeJson(result, response) ||
      !result["id_token"].is<const char *>() ||
      !result["refresh_token"].is<const char *>() ||
      !result["expires_in"].is<const char *>()) {
    Serial.println("Staging token refresh returned an invalid response.");
    scheduleAuthRetry();
    _state->tokenRefreshAt = _state->authRetryAt;
    return false;
  }
  if (result["user_id"].is<const char *>() &&
      result["user_id"].as<String>() != FIREBASE_DEVICE_UID) {
    Serial.println("Staging token refresh changed the device identity; access stopped.");
    _state->idToken = "";
    _state->refreshToken = "";
    _state->authenticated = false;
    scheduleAuthRetry();
    return false;
  }
  _state->idToken = result["id_token"].as<String>();
  _state->refreshToken = result["refresh_token"].as<String>();
  const uint32_t lifetime =
      strtoul(result["expires_in"].as<const char *>(), nullptr, 10);
  const uint32_t refreshIn =
      std::max<uint32_t>(kMinimumTokenLifetimeSeconds,
                         lifetime > kTokenRefreshMarginSeconds
                             ? lifetime - kTokenRefreshMarginSeconds
                             : kMinimumTokenLifetimeSeconds);
  _state->tokenRefreshAt = millis() + refreshIn * 1000UL;
  _state->authRetryDelay = 5000;
  _state->authenticated = true;
  return true;
}

bool FirebaseDiagnostics::ensureAuth() {
  if (!_state->authenticated) {
    if (!deadlineReached(millis(), _state->authRetryAt)) return false;
    return signIn();
  }
  if (deadlineReached(millis(), _state->tokenRefreshAt)) return refreshAuth();
  return true;
}

int FirebaseDiagnostics::databaseRequest(const String &method,
                                         const String &path,
                                         const String &query,
                                         const String &payload,
                                         String &response) {
  if (!_state->authenticated || WiFi.status() != WL_CONNECTED) return -1;
  int code =
      sendRequest(method, databaseUrl(path, query), payload, response);
  if (code == 401 && refreshAuth()) {
    code = sendRequest(method, databaseUrl(path, query), payload, response);
  }
  return code;
}

bool FirebaseDiagnostics::databaseGet(const String &path,
                                      const String &query,
                                      String &response) {
  return httpSuccess(databaseRequest("GET", path, query, "", response));
}

bool FirebaseDiagnostics::databasePatch(const String &path,
                                        const String &payload) {
  String response;
  return httpSuccess(databaseRequest("PATCH", path, "", payload, response));
}

bool FirebaseDiagnostics::databasePut(const String &path,
                                      const String &payload) {
  String response;
  return httpSuccess(databaseRequest("PUT", path, "", payload, response));
}

bool FirebaseDiagnostics::ensureDeviceRegistration() {
  if (_state->registered &&
      !deadlineReached(millis(), _state->registrationCheckAt)) {
    return true;
  }
  _state->registrationCheckAt = millis() + 30000;
  String response;
  const int code = databaseRequest(
      "GET", "deviceRegistry/" + String(FIREBASE_DEVICE_UID), "", "", response);
  if (!httpSuccess(code)) {
    _state->registered = false;
    Serial.printf("Staging device registry check failed (HTTP %d).\n", code);
    return false;
  }
  JsonDocument registry;
  if (deserializeJson(registry, response) ||
      registry["enabled"] != true ||
      registry["deviceId"].as<String>() != FIREBASE_DEVICE_ID) {
    _state->registered = false;
    Serial.println("Staging device registry entry is missing or disabled.");
    return false;
  }
  _state->registered = true;
  return true;
}

bool FirebaseDiagnostics::publishStatus(bool doorLocked, bool sensorHigh) {
  JsonDocument gateway;
  gateway["isOnline"] = true;
  gateway["lastSeen"] = nowEpochMs();
  gateway["mode"] = "esp32_only";
  gateway["deviceId"] = FIREBASE_DEVICE_ID;
  gateway["firmwareVersion"] = DEVICE_FIRMWARE_VERSION;
  String gatewayPayload;
  serializeJson(gateway, gatewayPayload);
  if (!databasePatch("devices/gateway", gatewayPayload)) return false;

  if (!databasePut("devices/frontDoor/isLocked", doorLocked ? "true" : "false")) {
    return false;
  }
  JsonDocument sensor;
  sensor["irDigitalState"] = sensorHigh ? "HIGH" : "LOW";
  sensor["lastSeen"] = nowEpochMs();
  String sensorPayload;
  serializeJson(sensor, sensorPayload);
  return databasePatch("devices/sensors", sensorPayload);
}

bool FirebaseDiagnostics::queueAudit(const String &type,
                                     const String &description,
                                     const String &commandId,
                                     const String &requestedBy,
                                     const String &stableId) {
  static const char *allowedTypes[] = {
      "remoteUnlock", "automaticRelock", "commandFailed", "commandExpired",
      "accessGranted", "accessDenied", "securityLockout"};
  bool allowed = false;
  for (const char *candidate : allowedTypes) {
    if (type == candidate) {
      allowed = true;
      break;
    }
  }
  if (!allowed || !_state->preferencesReady || description.length() > 200 ||
      commandId.length() > 128 || requestedBy.length() > 128 ||
      (!requestedBy.isEmpty() && requestedBy != FIREBASE_ADMIN_UID)) {
    Serial.println("Cloud audit event failed local schema validation.");
    return false;
  }

  const uint8_t count = _state->preferences.getUChar("auditCount", 0);
  const uint8_t head = _state->preferences.getUChar("auditHead", 0);
  String auditId = stableId;
  if (auditId.isEmpty()) {
    const uint64_t sequence =
        _state->preferences.getULong64("auditSeq", 0) + 1;
    if (_state->preferences.putULong64("auditSeq", sequence) == 0) return false;
    auditId = String(FIREBASE_DEVICE_ID) + "-event-" + String(sequence);
  }
  if (!validIdentifier(auditId, 128)) {
    Serial.println("Cloud audit event ID failed local validation.");
    return false;
  }
  for (uint8_t offset = 0; offset < count; ++offset) {
    const uint8_t slot = (head + offset) % kMaximumAuditQueueSize;
    const String queued =
        _state->preferences.getString(("e" + String(slot)).c_str(), "");
    const int separator = queued.indexOf('\n');
    if (separator > 0 && queued.substring(0, separator) == auditId) {
      return true;
    }
  }
  if (count >= kMaximumAuditQueueSize) {
    Serial.println("Cloud audit outbox full; remote command diagnostics paused.");
    return false;
  }

  JsonDocument record;
  record["type"] = type;
  record["description"] = description;
  record["timestamp"] = clockIsValid() ? nowEpochMs() : 0;
  record["commandId"] = commandId;
  record["actorUid"] = FIREBASE_DEVICE_UID;
  if (!requestedBy.isEmpty()) record["requestedBy"] = requestedBy;
  String json;
  serializeJson(record, json);
  const uint8_t tail = (head + count) % kMaximumAuditQueueSize;
  const String key = "e" + String(tail);
  if (_state->preferences.putString(key.c_str(), auditId + "\n" + json) == 0) {
    Serial.println("Could not persist cloud audit event.");
    return false;
  }
  if (_state->preferences.putUChar("auditCount", count + 1) == 0) {
    _state->preferences.remove(key.c_str());
    Serial.println("Could not persist cloud audit queue state.");
    return false;
  }
  return true;
}

bool FirebaseDiagnostics::recordEvent(const String &type,
                                      const String &description,
                                      const String &commandId,
                                      const String &requestedBy) {
  if (_state == nullptr || !_state->configured) return false;
  return queueAudit(type, description, commandId, requestedBy);
}

bool FirebaseDiagnostics::drainAuditQueue() {
  const uint8_t count = _state->preferences.getUChar("auditCount", 0);
  if (count == 0 || !clockIsValid() || !_state->registered) return true;
  const uint8_t head = _state->preferences.getUChar("auditHead", 0);
  const String key = "e" + String(head);
  const String queued = _state->preferences.getString(key.c_str(), "");
  const int separator = queued.indexOf('\n');
  if (separator <= 0) {
    Serial.println("Cloud audit outbox entry malformed; retained for recovery.");
    scheduleDatabaseRetry();
    return false;
  }
  String auditId = queued.substring(0, separator);
  JsonDocument record;
  if (deserializeJson(record, queued.substring(separator + 1))) {
    Serial.println("Cloud audit record invalid; retained for recovery.");
    scheduleDatabaseRetry();
    return false;
  }
  bool recordChanged = false;
  const String queuedActor = record["actorUid"].as<String>();
  if (queuedActor == FIREBASE_ADMIN_UID) {
    if (record["requestedBy"].isNull()) {
      record["requestedBy"] = queuedActor;
    }
    record["actorUid"] = FIREBASE_DEVICE_UID;
    const String migratedAuditId = auditId + "-device-actor";
    if (!validIdentifier(migratedAuditId, 128)) {
      Serial.println("Legacy audit event ID cannot be migrated; retained for recovery.");
      scheduleDatabaseRetry();
      return false;
    }
    auditId = migratedAuditId;
    recordChanged = true;
  } else if (!queuedActor.isEmpty() &&
             queuedActor != FIREBASE_DEVICE_UID) {
    Serial.println("Queued audit actor failed local validation; retained for recovery.");
    scheduleDatabaseRetry();
    return false;
  }
  const String requestedBy = record["requestedBy"].as<String>();
  if (!requestedBy.isEmpty() && requestedBy != FIREBASE_ADMIN_UID) {
    Serial.println("Queued audit requester failed local validation; retained for recovery.");
    scheduleDatabaseRetry();
    return false;
  }
  record["actorUid"] = FIREBASE_DEVICE_UID;
  if (record["timestamp"].as<uint64_t>() == 0) {
    record["timestamp"] = nowEpochMs();
    recordChanged = true;
  }
  if (recordChanged) {
    String updated;
    serializeJson(record, updated);
    if (_state->preferences.putString(key.c_str(), auditId + "\n" + updated) ==
        0) {
      Serial.println("Could not persist normalized cloud audit event.");
      return false;
    }
  }
  String payload;
  serializeJson(record, payload);
  if (!databasePut("auditLogs/" + auditId, payload)) {
    scheduleDatabaseRetry();
    return false;
  }
  _state->preferences.remove(key.c_str());
  _state->preferences.putUChar("auditHead", (head + 1) % kMaximumAuditQueueSize);
  _state->preferences.putUChar("auditCount", count - 1);
  _state->auditRetryDelay = 1000;
  _state->auditRetryAt = millis() + 1000;
  return true;
}

bool FirebaseDiagnostics::processPendingCommand() {
  const String query =
      "orderBy=%22status%22&equalTo=%22pending%22&limitToFirst=10";
  String response;
  if (!databaseGet("commands", query, response)) return false;
  JsonDocument commands;
  if (deserializeJson(commands, response) ||
      !commands.is<JsonObjectConst>()) {
    Serial.println("Staging command query returned invalid JSON.");
    return false;
  }

  for (JsonPairConst entry : commands.as<JsonObjectConst>()) {
    const JsonObjectConst record = entry.value().as<JsonObjectConst>();
    const String commandId = entry.key().c_str();
    if (!validIdentifier(commandId, 32) || record["status"] != "pending") {
      continue;
    }
    if (record["targetDeviceId"].as<String>() != FIREBASE_DEVICE_ID) continue;
    const bool validShape =
        record.size() == 6 && record["type"] == "unlock" &&
        record["requestedBy"].as<String>() == FIREBASE_ADMIN_UID &&
        record["createdAt"].is<uint64_t>() &&
        record["expiresAt"].is<uint64_t>();
    if (!validShape) {
      Serial.println("Targeted staging command failed device-side validation.");
      continue;
    }

    const uint64_t createdAt = record["createdAt"].as<uint64_t>();
    const uint64_t expiresAt = record["expiresAt"].as<uint64_t>();
    const uint64_t now = nowEpochMs();
    const bool expired = expiresAt <= now;
    const bool timeValid =
        createdAt <= now + 5000 && expiresAt > createdAt &&
        expiresAt <= createdAt + kMaximumCommandLifetimeMs;
    const String status = expired ? "expired" : "failed";
    const String message =
        expired
            ? "Command expired before device processing."
            : !timeValid
                ? "Command timestamp or expiry failed device validation."
                : "Command received; remote servo actuation is disabled in this firmware build.";
    const String requestedBy = record["requestedBy"].as<String>();
    const String stableId =
        String(FIREBASE_DEVICE_ID) + "-cmd-" + commandId + "-" + status;
    if (!queueAudit(expired ? "commandExpired" : "commandFailed", message,
                    commandId, requestedBy, stableId)) {
      return false;
    }

    JsonDocument patch;
    patch["status"] = status;
    patch["acknowledgedAt"] = now;
    patch["deviceId"] = FIREBASE_DEVICE_ID;
    patch["message"] = message;
    String payload;
    serializeJson(patch, payload);
    if (!databasePatch("commands/" + commandId, payload)) {
      Serial.println("Staging command acknowledgement failed; it will be retried.");
      return false;
    }
    Serial.printf("Staging command %s acknowledged (%s); no servo movement occurred.\n",
                  commandId.c_str(), status.c_str());
    return true;
  }
  return true;
}

void FirebaseDiagnostics::tick(bool doorLocked, bool sensorHigh) {
  if (_state == nullptr || !_state->configured) return;
  maintainWifi();
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected != _state->lastWifiConnected) {
    Serial.println(connected ? "Staging Wi-Fi connected."
                             : "Staging Wi-Fi disconnected; local keypad remains active.");
    _state->lastWifiConnected = connected;
    if (connected) {
      _state->wifiRetryDelay = 2000;
      _state->wifiRetryAt = millis();
      _state->authenticated = false;
      _state->registered = false;
      _state->authRetryAt = millis();
    }
  }
  if (!connected || !clockIsValid() || !ensureAuth() ||
      !ensureDeviceRegistration()) {
    return;
  }

  if (deadlineReached(millis(), _state->heartbeatAt)) {
    if (publishStatus(doorLocked, sensorHigh)) {
      _state->heartbeatAt = millis() + kHeartbeatIntervalMs;
    } else {
      _state->heartbeatAt = millis() + kHeartbeatIntervalMs;
      Serial.println("Staging status sync failed; the app will mark stale heartbeat offline.");
    }
  }
  if (deadlineReached(millis(), _state->commandPollAt)) {
    _state->commandPollAt = millis() + kCommandPollIntervalMs;
    processPendingCommand();
  }
  if (deadlineReached(millis(), _state->auditRetryAt)) drainAuditQueue();
}

#else

class FirebaseDiagnostics::State {};

FirebaseDiagnostics::~FirebaseDiagnostics() = default;

void FirebaseDiagnostics::begin() {}
void FirebaseDiagnostics::tick(bool, bool) {}
bool FirebaseDiagnostics::recordEvent(const String &, const String &,
                                      const String &, const String &) {
  return false;
}

#endif
