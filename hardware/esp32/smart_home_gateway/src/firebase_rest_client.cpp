#include "firebase_rest_client.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <algorithm>
#include <cstring>
#include <sys/time.h>

#include "device_config.h"

namespace {
constexpr uint32_t kCommandPollIntervalMs = 1000;
constexpr uint32_t kHeartbeatIntervalMs = 5000;
constexpr uint32_t kRegistrationCheckIntervalMs = 30000;
constexpr uint32_t kCommandAckTimeoutMs = 3000;
constexpr uint32_t kMinimumTokenLifetimeSeconds = 60;
constexpr uint32_t kTokenRefreshMarginSeconds = 300;
constexpr uint32_t kMaximumCommandLifetimeMs = 60000;
constexpr uint8_t kMaximumAuditQueueSize = 8;
constexpr uint8_t kDefaultUnlockDurationSeconds = 4;
constexpr uint32_t kMinimumUnlockDurationSeconds = 4;
constexpr uint32_t kMaximumUnlockDurationSeconds = 30;
constexpr uint64_t kMinimumValidEpochMs = 1700000000000ULL;
constexpr uint64_t kMaximumValidEpochMs = 2051222400000ULL;

bool isHttpSuccess(int code) { return code >= 200 && code < 300; }
}  // namespace

void FirebaseRestClient::begin() {
  _preferencesReady = _preferences.begin("iot-gateway", false);
  if (!_preferencesReady) {
    Serial.println("NVS unavailable; cloud command processing is disabled.");
    return;
  }
  _activeCommandId = _preferences.getString("activeId", "");
  _activePhase = _preferences.getString("activePhase", "");
  _activeFailureMessage = _preferences.getString("activeMessage", "");
  _lastRemoteCommandId = _preferences.getString("remoteId", "");
  _lastRemoteActorUid = _preferences.getString("remoteActor", "");
  _recoveryPending = !_activeCommandId.isEmpty();
  if (!isConfigured()) {
    Serial.println("Firebase device configuration is incomplete; no cloud connection will be attempted.");
    return;
  }
  Serial.println(FIREBASE_USE_EMULATOR
                     ? "Firebase REST client configured for an emulator."
                     : "Firebase REST client configured for TLS-verified endpoints.");
}

bool FirebaseRestClient::isConfigured() const {
  if (!FIREBASE_DEVICE_EMAIL[0] || !FIREBASE_DEVICE_PASSWORD[0] ||
      !FIREBASE_API_KEY[0] || !FIREBASE_PROJECT_ID[0] ||
      !FIREBASE_DEVICE_ID[0] || !FIREBASE_ADMIN_UID[0] ||
      !validIdentifier(FIREBASE_DEVICE_ID, 64)) {
    return false;
  }
#if FIREBASE_USE_EMULATOR
  return FIREBASE_AUTH_EMULATOR_URL[0] &&
         FIREBASE_DATABASE_EMULATOR_URL[0] &&
         String(FIREBASE_AUTH_EMULATOR_URL).startsWith("http://") &&
         String(FIREBASE_DATABASE_EMULATOR_URL).startsWith("http://");
#else
  return String(FIREBASE_AUTH_BASE_URL).startsWith("https://") &&
         String(FIREBASE_TOKEN_BASE_URL).startsWith("https://") &&
         String(FIREBASE_DATABASE_URL).startsWith("https://");
#endif
}

uint64_t FirebaseRestClient::nowEpochMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return static_cast<uint64_t>(tv.tv_sec) * 1000ULL +
         static_cast<uint64_t>(tv.tv_usec / 1000);
}

bool FirebaseRestClient::clockIsValid() {
  const uint64_t now = nowEpochMs();
  return now >= kMinimumValidEpochMs && now <= kMaximumValidEpochMs;
}

bool FirebaseRestClient::deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

bool FirebaseRestClient::validIdentifier(const String &value,
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

String FirebaseRestClient::encodeUrl(const String &value) const {
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

String FirebaseRestClient::authUrl(const String &endpoint) const {
#if FIREBASE_USE_EMULATOR
  String base = FIREBASE_AUTH_EMULATOR_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/identitytoolkit.googleapis.com/v1/" + endpoint +
         "?key=" + encodeUrl(FIREBASE_API_KEY);
#else
  String base = FIREBASE_AUTH_BASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/v1/" + endpoint + "?key=" +
         encodeUrl(FIREBASE_API_KEY);
#endif
}

String FirebaseRestClient::tokenUrl() const {
#if FIREBASE_USE_EMULATOR
  String base = FIREBASE_AUTH_EMULATOR_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/securetoken.googleapis.com/v1/token?key=" +
         encodeUrl(FIREBASE_API_KEY);
#else
  String base = FIREBASE_TOKEN_BASE_URL;
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/v1/token?key=" + encodeUrl(FIREBASE_API_KEY);
#endif
}

String FirebaseRestClient::databaseUrl(const String &path,
                                       const String &query) const {
  String base;
#if FIREBASE_USE_EMULATOR
  base = FIREBASE_DATABASE_EMULATOR_URL;
#else
  base = FIREBASE_DATABASE_URL;
#endif
  while (base.endsWith("/")) base.remove(base.length() - 1);
  const int queryIndex = base.indexOf('?');
  String baseQuery;
  if (queryIndex >= 0) {
    baseQuery = base.substring(queryIndex + 1);
    base = base.substring(0, queryIndex);
  }
  String url = base + "/" + path + ".json";
  bool hasQuery = false;
  if (!baseQuery.isEmpty()) {
    url += "?" + baseQuery;
    hasQuery = true;
  }
  if (!query.isEmpty()) {
    url += hasQuery ? "&" : "?";
    url += query;
    hasQuery = true;
  }
  if (!_idToken.isEmpty()) {
    url += hasQuery ? "&" : "?";
    url += "auth=" + encodeUrl(_idToken);
  }
  return url;
}

int FirebaseRestClient::sendRequest(const String &method, const String &url,
                                    const String &payload, String &response,
                                    const char *contentType,
                                    const char *headerName,
                                    const char *headerValue) {
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
#if FIREBASE_USE_EMULATOR
  WiFiClient client;
  if (!http.begin(client, url)) return HTTPC_ERROR_CONNECTION_REFUSED;
#else
  WiFiClientSecure client;
  client.setHandshakeTimeout(5);
  client.useBuiltinCACertBundle();
  if (!http.begin(client, url)) return HTTPC_ERROR_CONNECTION_REFUSED;
#endif
  http.addHeader("Accept", "application/json");
  if (contentType != nullptr) http.addHeader("Content-Type", contentType);
  if (headerName != nullptr && headerValue != nullptr) {
    http.addHeader(headerName, headerValue);
  }
  const int code = http.sendRequest(method.c_str(), payload);
  if (code > 0) response = http.getString();
  http.end();
  return code;
}

void FirebaseRestClient::scheduleAuthRetry() {
  _authRetryAtMillis = millis() + _authRetryDelayMs;
  _authRetryDelayMs = std::min<uint32_t>(_authRetryDelayMs * 2, 60000);
}

bool FirebaseRestClient::signIn() {
  JsonDocument request;
  request["email"] = FIREBASE_DEVICE_EMAIL;
  request["password"] = FIREBASE_DEVICE_PASSWORD;
  request["returnSecureToken"] = true;
  String payload;
  serializeJson(request, payload);
  String response;
  const int code = sendRequest(
      "POST", authUrl("accounts:signInWithPassword"), payload, response);
  if (!isHttpSuccess(code)) {
    Serial.printf("Firebase device sign-in failed (HTTP %d).\n", code);
    _authenticated = false;
    scheduleAuthRetry();
    return false;
  }

  JsonDocument result;
  if (deserializeJson(result, response) ||
      !result["idToken"].is<const char *>() ||
      !result["refreshToken"].is<const char *>() ||
      !result["localId"].is<const char *>() ||
      !result["expiresIn"].is<const char *>()) {
    Serial.println("Firebase device sign-in returned an invalid token response.");
    _authenticated = false;
    scheduleAuthRetry();
    return false;
  }

  const String uid = result["localId"].as<String>();
  if (uid.isEmpty() || uid == FIREBASE_ADMIN_UID ||
      uid == "device-simulator") {
    Serial.println("Firebase identity is not an approved dedicated device user.");
    _authenticated = false;
    scheduleAuthRetry();
    return false;
  }
  _idToken = result["idToken"].as<String>();
  _refreshToken = result["refreshToken"].as<String>();
  _deviceUid = uid;
  const uint32_t expiresIn = strtoul(
      result["expiresIn"].as<const char *>(), nullptr, 10);
  const uint32_t refreshIn =
      std::max<uint32_t>(kMinimumTokenLifetimeSeconds,
                    expiresIn > kTokenRefreshMarginSeconds
                        ? expiresIn - kTokenRefreshMarginSeconds
                        : kMinimumTokenLifetimeSeconds);
  _tokenRefreshAtMillis = millis() + refreshIn * 1000UL;
  _authRetryDelayMs = 5000;
  _authenticated = true;
  _registrationCheckAtMillis = 0;
  Serial.println("Dedicated Firebase device identity authenticated.");
  return true;
}

bool FirebaseRestClient::refreshAuth() {
  if (_refreshToken.isEmpty()) {
    _authenticated = false;
    return signIn();
  }
  const String payload = "grant_type=refresh_token&refresh_token=" +
                         encodeUrl(_refreshToken);
  String response;
  const int code = sendRequest("POST", tokenUrl(), payload, response,
                               "application/x-www-form-urlencoded");
  if (!isHttpSuccess(code)) {
    Serial.printf("Firebase device token refresh failed (HTTP %d).\n", code);
    if (code == 400 || code == 401) {
      _idToken = "";
      _refreshToken = "";
      _authenticated = false;
    }
    scheduleAuthRetry();
    _tokenRefreshAtMillis = _authRetryAtMillis;
    return false;
  }

  JsonDocument result;
  if (deserializeJson(result, response) ||
      !result["id_token"].is<const char *>() ||
      !result["refresh_token"].is<const char *>() ||
      !result["expires_in"].is<const char *>()) {
    Serial.println("Firebase token refresh returned an invalid response.");
    scheduleAuthRetry();
    _tokenRefreshAtMillis = _authRetryAtMillis;
    return false;
  }
  if (result["user_id"].is<const char *>() &&
      result["user_id"].as<String>() != _deviceUid) {
    Serial.println("Firebase refresh token changed device identity; stopping cloud access.");
    _authenticated = false;
    _idToken = "";
    _refreshToken = "";
    scheduleAuthRetry();
    return false;
  }
  _idToken = result["id_token"].as<String>();
  _refreshToken = result["refresh_token"].as<String>();
  const uint32_t expiresIn =
      strtoul(result["expires_in"].as<const char *>(), nullptr, 10);
  const uint32_t refreshIn =
      std::max<uint32_t>(kMinimumTokenLifetimeSeconds,
                    expiresIn > kTokenRefreshMarginSeconds
                        ? expiresIn - kTokenRefreshMarginSeconds
                        : kMinimumTokenLifetimeSeconds);
  _tokenRefreshAtMillis = millis() + refreshIn * 1000UL;
  _authRetryDelayMs = 5000;
  _authenticated = true;
  return true;
}

bool FirebaseRestClient::ensureAuth() {
  if (!_authenticated) {
    if (!deadlineReached(millis(), _authRetryAtMillis)) return false;
    return signIn();
  }
  if (deadlineReached(millis(), _tokenRefreshAtMillis)) {
    return refreshAuth();
  }
  return true;
}

void FirebaseRestClient::scheduleDatabaseRetry(uint32_t &deadline,
                                               uint32_t &delayMs) {
  deadline = millis() + delayMs;
  delayMs = std::min<uint32_t>(delayMs * 2, 30000);
}

bool FirebaseRestClient::ensureDeviceRegistration() {
  if (!_registered && !deadlineReached(millis(), _registrationCheckAtMillis)) {
    return false;
  }
  if (_registered &&
      !deadlineReached(millis(), _registrationCheckAtMillis)) {
    return true;
  }
  _registrationCheckAtMillis =
      millis() + kRegistrationCheckIntervalMs;
  if (!_authenticated || _deviceUid.isEmpty()) return false;

  String response;
  const int code = databaseRequest(
      "GET", "deviceRegistry/" + _deviceUid, "", "", response);
  if (!isHttpSuccess(code)) {
    _registered = false;
    Serial.printf("Firebase device registry check failed (HTTP %d).\n", code);
    return false;
  }
  JsonDocument registry;
  if (deserializeJson(registry, response) ||
      registry["enabled"] != true ||
      registry["deviceId"].as<String>() != FIREBASE_DEVICE_ID) {
    _registered = false;
    Serial.println("Firebase device is not enabled in its registry entry.");
    return false;
  }
  const bool wasRegistered = _registered;
  _registered = true;
  if (!wasRegistered) Serial.println("Firebase device registry accepted.");
  return true;
}

int FirebaseRestClient::databaseRequest(const String &method,
                                        const String &path,
                                        const String &query,
                                        const String &payload,
                                        String &response,
                                        const char *headerName,
                                        const char *headerValue) {
  if (!_authenticated || WiFi.status() != WL_CONNECTED) return -1;
  int code = sendRequest(method, databaseUrl(path, query), payload, response,
                         "application/json", headerName, headerValue);
  if (code == 401 && refreshAuth()) {
    code = sendRequest(method, databaseUrl(path, query), payload, response,
                       "application/json", headerName, headerValue);
  }
  return code;
}

bool FirebaseRestClient::databaseGet(const String &path, const String &query,
                                     String &response) {
  const int code = databaseRequest("GET", path, query, "", response);
  if (!isHttpSuccess(code)) {
    Serial.printf("Firebase read failed for %s (HTTP %d).\n", path.c_str(),
                  code);
    return false;
  }
  return true;
}

bool FirebaseRestClient::databasePatch(const String &path,
                                       const JsonDocument &document) {
  String payload;
  serializeJson(document, payload);
  String response;
  const int code =
      databaseRequest("PATCH", path, "", payload, response);
  if (!isHttpSuccess(code)) {
    Serial.printf("Firebase update failed for %s (HTTP %d).\n", path.c_str(),
                  code);
    return false;
  }
  return true;
}

bool FirebaseRestClient::databasePut(const String &path,
                                     const String &payload,
                                     const char *headerName,
                                     const char *headerValue) {
  String response;
  const int code = databaseRequest("PUT", path, "", payload, response,
                                   headerName, headerValue);
  if (!isHttpSuccess(code) && code != 412) {
    Serial.printf("Firebase write failed for %s (HTTP %d).\n", path.c_str(),
                  code);
    return false;
  }
  return true;
}

bool FirebaseRestClient::readCommand(const String &commandId,
                                     JsonDocument &record) {
  String response;
  if (!databaseGet("commands/" + commandId, "", response)) return false;
  if (deserializeJson(record, response)) {
    Serial.println("Firebase command record was not valid JSON.");
    return false;
  }
  return true;
}

bool FirebaseRestClient::writeCommandPatch(const String &commandId,
                                           JsonDocument &patch) {
  return databasePatch("commands/" + commandId, patch);
}

void FirebaseRestClient::clearActive() {
  if (_preferencesReady) {
    _preferences.remove("activeId");
    _preferences.remove("activePhase");
    _preferences.remove("activeMessage");
  }
  _activeCommandId = "";
  _activeActorUid = "";
  _activePhase = "";
  _activeFailureMessage = "";
  _activeExpiresAtEpochMs = 0;
  _activeAckAccepted = false;
  _activeRemoteStateSeen = false;
  _activeStartedAtMillis = 0;
  _recoveryPending = false;
}

bool FirebaseRestClient::recoverActiveCommand() {
  if (!_recoveryPending || _activeCommandId.isEmpty()) return true;
  JsonDocument command;
  if (!readCommand(_activeCommandId, command)) return false;
  const String status = command["status"] | "";
  if (status == "pending" || status == "completed" || status == "failed" ||
      status == "expired" || command.isNull()) {
    clearActive();
    return true;
  }
  const String deviceId = command["deviceId"] | "";
  if (status != "accepted" || deviceId != FIREBASE_DEVICE_ID) {
    Serial.println("Stored command recovery does not match this device; commands remain disabled.");
    return false;
  }
  _activeActorUid = command["requestedBy"] | "";
  _activeExpiresAtEpochMs = command["expiresAt"].as<uint64_t>();
  _activeStartedAtMillis = millis();
  _recoveryPending = false;
  if (_activePhase == "executed") {
    _activeRemoteStateSeen = true;
    return completeActiveCommand();
  }
  _activeFailureMessage =
      "Gateway restarted before execution acknowledgement; command was not replayed.";
  _activePhase = "failed";
  if (_preferencesReady) _preferences.putString("activePhase", _activePhase);
  if (_preferencesReady) {
    _preferences.putString("activeMessage", _activeFailureMessage);
  }
  return applyActiveFailure();
}

void FirebaseRestClient::tick(bool arduinoReady, bool doorStateKnown,
                              bool doorLocked,
                              uint64_t arduinoLastSeenEpochMs) {
  if (!_preferencesReady || !isConfigured()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (!clockIsValid()) return;
  if (!ensureAuth() || !ensureDeviceRegistration()) return;
  if (!recoverActiveCommand()) return;

  if (doorStateKnown) {
    if (!_doorStateKnown || _doorLocked != doorLocked) {
      _doorStateKnown = true;
      _doorLocked = doorLocked;
      _doorStateDirty = true;
      _doorRetryAtMillis = 0;
      _doorRetryDelayMs = 1000;
    }
  }

  if (_activePhase == "executed") completeActiveCommand();
  if (_activePhase == "failed" || _activePhase == "expired") {
    applyActiveFailure();
  } else if (!_activeCommandId.isEmpty() && _activeStartedAtMillis != 0 &&
             deadlineReached(millis(),
                             _activeStartedAtMillis + kCommandAckTimeoutMs)) {
    failActiveCommand(
        "Arduino did not confirm command execution before the acknowledgement timeout.");
  }

  if (clockIsValid() &&
      deadlineReached(millis(), _heartbeatAtMillis)) {
    updateGatewayHeartbeat(arduinoReady, arduinoLastSeenEpochMs);
    _heartbeatAtMillis = millis() + kHeartbeatIntervalMs;
  }
  if (_doorStateDirty && clockIsValid() &&
      deadlineReached(millis(), _doorRetryAtMillis)) {
    updateDoorState();
  }
  if (clockIsValid() &&
      deadlineReached(millis(), _auditRetryAtMillis)) {
    drainAuditQueue();
  }
}

bool FirebaseRestClient::readyForCommands() {
  const uint8_t count = _preferencesReady
                            ? _preferences.getUChar("auditCount", 0)
                            : kMaximumAuditQueueSize;
  return _preferencesReady && isConfigured() &&
         WiFi.status() == WL_CONNECTED && _authenticated && _registered &&
         clockIsValid() && _activeCommandId.isEmpty() && !_recoveryPending &&
         count < kMaximumAuditQueueSize;
}

bool FirebaseRestClient::readUnlockDuration(uint32_t &durationMs) {
  String response;
  if (!databaseGet("settings/frontDoor/unlockDurationSeconds", "",
                   response)) {
    return false;
  }
  JsonDocument setting;
  if (deserializeJson(setting, response)) {
    Serial.println("Unlock duration setting returned invalid JSON.");
    return false;
  }
  if (setting.isNull()) {
    durationMs = kDefaultUnlockDurationSeconds * 1000UL;
    return true;
  }
  if (!setting.is<uint32_t>()) {
    Serial.println("Unlock duration setting is not an integer.");
    return false;
  }
  const uint32_t seconds = setting.as<uint32_t>();
  if (seconds < kMinimumUnlockDurationSeconds ||
      seconds > kMaximumUnlockDurationSeconds) {
    Serial.println("Unlock duration setting is outside the supported 4-30 second range.");
    return false;
  }
  durationMs = seconds * 1000UL;
  return true;
}

bool FirebaseRestClient::rejectPendingCommand(const CloudCommand &command,
                                              const String &status,
                                              const String &message) {
  const String eventType = status == "expired" ? "commandExpired" : "commandFailed";
  if (!queueAudit(eventType, message, command.id, command.requestedBy,
                  String(FIREBASE_DEVICE_ID) + "-cmd-" + command.id + "-" +
                      status)) {
    Serial.println("Audit outbox full; pending command was not changed.");
    return false;
  }
  JsonDocument patch;
  patch["status"] = status;
  patch["acknowledgedAt"] = nowEpochMs();
  patch["deviceId"] = FIREBASE_DEVICE_ID;
  patch["message"] = message;
  return writeCommandPatch(command.id, patch);
}

bool FirebaseRestClient::beginNextCommand(CloudCommand &command) {
  if (!readyForCommands() ||
      !deadlineReached(millis(), _commandPollAtMillis)) {
    return false;
  }
  _commandPollAtMillis = millis() + kCommandPollIntervalMs;
  const String query =
      "orderBy=%22status%22&equalTo=%22pending%22&limitToFirst=10";
  String response;
  if (!databaseGet("commands", query, response)) return false;
  JsonDocument commands;
  if (deserializeJson(commands, response)) {
    Serial.println("Pending command query returned invalid JSON.");
    return false;
  }
  if (!commands.is<JsonObjectConst>()) return false;

  const uint64_t now = nowEpochMs();
  for (JsonPairConst entry : commands.as<JsonObjectConst>()) {
    JsonObjectConst record = entry.value().as<JsonObjectConst>();
    if (record.isNull() || record["status"] != "pending") continue;
    command.id = entry.key().c_str();
    command.requestedBy = record["requestedBy"] | "";
    const bool shapeValid =
        validIdentifier(command.id, 32) && record.size() == 5 &&
        record["type"] == "unlock" &&
        record["createdAt"].is<uint64_t>() &&
        record["expiresAt"].is<uint64_t>() &&
        !command.requestedBy.isEmpty();
    if (!shapeValid) {
      if (validIdentifier(command.id, 32)) {
        rejectPendingCommand(command, "failed",
                             "Malformed pending command record.");
      } else {
        Serial.println("Pending Firebase command has an invalid ID; it was not sent to the Uno.");
      }
      return false;
    }
    const uint64_t createdAt = record["createdAt"].as<uint64_t>();
    command.expiresAtEpochMs = record["expiresAt"].as<uint64_t>();
    if (command.requestedBy != FIREBASE_ADMIN_UID ||
        createdAt > now + 5000 || command.expiresAtEpochMs <= createdAt ||
        command.expiresAtEpochMs > createdAt + kMaximumCommandLifetimeMs) {
      rejectPendingCommand(command, "failed",
                           "Pending command failed device-side validation.");
      return false;
    }
    if (command.expiresAtEpochMs <= now) {
      rejectPendingCommand(command, "expired",
                           "Command expired before device acceptance.");
      return false;
    }
    if (!readUnlockDuration(command.unlockDurationMs)) {
      Serial.println("Command remains pending because its unlock setting could not be verified.");
      return false;
    }

    if (!persistActive(command.id, "claiming")) return false;
    _activeActorUid = command.requestedBy;
    _activeExpiresAtEpochMs = command.expiresAtEpochMs;
    JsonDocument patch;
    patch["status"] = "accepted";
    patch["acceptedAt"] = nowEpochMs();
    patch["deviceId"] = FIREBASE_DEVICE_ID;
    if (writeCommandPatch(command.id, patch)) {
      persistActive(command.id, "accepted");
      _activeStartedAtMillis = 0;
      _activeAckAccepted = false;
      _activeRemoteStateSeen = false;
      Serial.println("Firebase command claimed; waiting for Uno acknowledgement.");
      return true;
    }

    JsonDocument current;
    const bool commandRead = readCommand(command.id, current);
    if (commandRead && current["status"] == "accepted" &&
        current["deviceId"] == FIREBASE_DEVICE_ID) {
      persistActive(command.id, "accepted");
      _activeStartedAtMillis = 0;
      _activeAckAccepted = false;
      _activeRemoteStateSeen = false;
      Serial.println("Firebase command claim was confirmed after a lost response.");
      return true;
    }
    if (!commandRead) {
      _recoveryPending = true;
      return false;
    }
    clearActive();
    return false;
  }
  return false;
}

void FirebaseRestClient::noteCommandSent() {
  if (_activeCommandId.isEmpty()) return;
  _activeStartedAtMillis = millis();
}

void FirebaseRestClient::failActiveCommand(const String &reason) {
  if (_activeCommandId.isEmpty()) return;
  _activeFailureMessage = reason;
  _activePhase = _activeExpiresAtEpochMs > 0 &&
                         _activeExpiresAtEpochMs <= nowEpochMs()
                     ? "expired"
                     : "failed";
  if (_preferencesReady) {
    _preferences.putString("activePhase", _activePhase);
    _preferences.putString("activeMessage", _activeFailureMessage);
  }
  applyActiveFailure();
}

bool FirebaseRestClient::applyActiveFailure() {
  if (_activeCommandId.isEmpty() ||
      (_activePhase != "failed" && _activePhase != "expired")) {
    return true;
  }
  const String status = _activePhase;
  const String eventType =
      status == "expired" ? "commandExpired" : "commandFailed";
  if (!queueAudit(eventType, _activeFailureMessage, _activeCommandId,
                  _activeActorUid,
                  String(FIREBASE_DEVICE_ID) + "-cmd-" + _activeCommandId +
                      "-" + status)) {
    Serial.println("Audit outbox full; command failure is queued in NVS.");
    return false;
  }
  JsonDocument patch;
  patch["status"] = status;
  patch["acknowledgedAt"] = nowEpochMs();
  patch["deviceId"] = FIREBASE_DEVICE_ID;
  patch["message"] = _activeFailureMessage;
  if (writeCommandPatch(_activeCommandId, patch)) {
    clearActive();
    return true;
  }
  JsonDocument current;
  if (readCommand(_activeCommandId, current) &&
      current["status"] == status &&
      current["deviceId"] == FIREBASE_DEVICE_ID) {
    clearActive();
    return true;
  }
  return false;
}

bool FirebaseRestClient::completeActiveCommand() {
  if (_activeCommandId.isEmpty() || _activePhase != "executed") return false;
  if (_preferencesReady &&
      (_preferences.putString("remoteId", _activeCommandId) == 0 ||
       _preferences.putString("remoteActor", _activeActorUid) == 0)) {
    Serial.println("Could not persist remote command audit linkage.");
    return false;
  }
  _lastRemoteCommandId = _activeCommandId;
  _lastRemoteActorUid = _activeActorUid;
  const String eventId =
      String(FIREBASE_DEVICE_ID) + "-cmd-" + _activeCommandId + "-remote";
  if (!queueAudit("remoteUnlock",
                  "Arduino acknowledged execution of the remote unlock.",
                  _activeCommandId, _activeActorUid, eventId)) {
    Serial.println("Audit outbox full; completed command remains pending in NVS.");
    return false;
  }
  JsonDocument patch;
  patch["status"] = "completed";
  patch["acknowledgedAt"] = nowEpochMs();
  patch["message"] = "Arduino acknowledged the servo command.";
  if (!writeCommandPatch(_activeCommandId, patch)) {
    JsonDocument current;
    if (!readCommand(_activeCommandId, current) ||
        current["status"] != "completed" ||
        current["deviceId"] != FIREBASE_DEVICE_ID ||
        !current["acknowledgedAt"].is<uint64_t>()) {
      return false;
    }
  }
  Serial.println("Arduino execution acknowledgement recorded in Firebase.");
  clearActive();
  return true;
}

bool FirebaseRestClient::persistActive(const String &commandId,
                                       const String &phase) {
  if (!_preferencesReady) return false;
  if (_preferences.putString("activeId", commandId) == 0 ||
      _preferences.putString("activePhase", phase) == 0) {
    Serial.println("Could not persist active command state to NVS.");
    return false;
  }
  _activeCommandId = commandId;
  _activePhase = phase;
  _recoveryPending = false;
  return true;
}

bool FirebaseRestClient::queueAudit(const String &type,
                                    const String &description,
                                    const String &commandId,
                                    const String &actorUid,
                                    const String &stableAuditId) {
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
  if (!allowed || !_preferencesReady || description.length() > 200 ||
      commandId.length() > 128 || actorUid.length() > 128) {
    Serial.println("Audit event failed local schema validation.");
    return false;
  }

  const uint8_t count = _preferences.getUChar("auditCount", 0);
  const uint8_t head = _preferences.getUChar("auditHead", 0);
  String auditId = stableAuditId;
  if (auditId.isEmpty()) {
    const uint64_t sequence = _preferences.getULong64("auditSeq", 0) + 1;
    if (_preferences.putULong64("auditSeq", sequence) == 0) return false;
    auditId = String(FIREBASE_DEVICE_ID) + "-event-" + String(sequence);
  }
  if (!validIdentifier(auditId, 128)) {
    Serial.println("Audit event ID failed local validation.");
    return false;
  }
  for (uint8_t offset = 0; offset < count; ++offset) {
    const uint8_t slot = (head + offset) % kMaximumAuditQueueSize;
    const String queued =
        _preferences.getString(("e" + String(slot)).c_str(), "");
    const int separator = queued.indexOf('\n');
    if (separator > 0 && queued.substring(0, separator) == auditId) {
      return true;
    }
  }
  if (count >= kMaximumAuditQueueSize) {
    Serial.println("Persistent audit outbox is full; remote commands are disabled.");
    return false;
  }

  JsonDocument record;
  record["type"] = type;
  record["description"] = description;
  record["timestamp"] = clockIsValid() ? nowEpochMs() : 0;
  record["commandId"] = commandId;
  record["actorUid"] = actorUid;
  String json;
  serializeJson(record, json);
  const uint8_t tail =
      (head + count) % kMaximumAuditQueueSize;
  const String key = "e" + String(tail);
  if (_preferences.putString(key.c_str(), auditId + "\n" + json) == 0) {
    Serial.println("Could not persist audit event to NVS.");
    return false;
  }
  if (_preferences.putUChar("auditCount", count + 1) == 0) {
    _preferences.remove(key.c_str());
    Serial.println("Could not persist audit event to NVS.");
    return false;
  }
  return true;
}

void FirebaseRestClient::drainAuditQueue() {
  const uint8_t count = _preferences.getUChar("auditCount", 0);
  if (count == 0 || !clockIsValid() || !_registered) return;
  const uint8_t head = _preferences.getUChar("auditHead", 0);
  const String key = "e" + String(head);
  String queued = _preferences.getString(key.c_str(), "");
  const int separator = queued.indexOf('\n');
  if (separator <= 0) {
    Serial.println("Persistent audit outbox entry is malformed; it was retained for recovery.");
    scheduleDatabaseRetry(_auditRetryAtMillis, _auditRetryDelayMs);
    return;
  }
  const String auditId = queued.substring(0, separator);
  JsonDocument record;
  if (deserializeJson(record, queued.substring(separator + 1))) {
    Serial.println("Persistent audit record is invalid JSON; it was retained.");
    scheduleDatabaseRetry(_auditRetryAtMillis, _auditRetryDelayMs);
    return;
  }
  if (record["timestamp"].as<uint64_t>() == 0) {
    record["timestamp"] = nowEpochMs();
  }
  String payload;
  serializeJson(record, payload);
  if (!databasePut("auditLogs/" + auditId, payload)) {
    scheduleDatabaseRetry(_auditRetryAtMillis, _auditRetryDelayMs);
    return;
  }
  _preferences.remove(key.c_str());
  _preferences.putUChar("auditHead",
                        (head + 1) % kMaximumAuditQueueSize);
  _preferences.putUChar("auditCount", count - 1);
  _auditRetryDelayMs = 1000;
  _auditRetryAtMillis = millis() + 1000;
}

void FirebaseRestClient::updateDoorState() {
  if (!_registered || !_doorStateKnown) return;
  const bool written = databasePut(
      "devices/frontDoor/isLocked", _doorLocked ? "true" : "false");
  if (written) {
    _doorStateDirty = false;
    _doorRetryDelayMs = 1000;
    _doorRetryAtMillis = millis();
  } else {
    scheduleDatabaseRetry(_doorRetryAtMillis, _doorRetryDelayMs);
  }
}

void FirebaseRestClient::updateGatewayHeartbeat(
    bool arduinoReady, uint64_t arduinoLastSeenEpochMs) {
  JsonDocument patch;
  patch["isOnline"] = true;
  patch["lastSeen"] = nowEpochMs();
  patch["mode"] = "hardware";
  patch["arduinoConnected"] = arduinoReady;
  if (arduinoLastSeenEpochMs > 0) {
    patch["arduinoLastSeen"] = arduinoLastSeenEpochMs;
  }
  if (!databasePatch("devices/gateway", patch)) {
    Serial.println("Gateway heartbeat was not accepted; the app will mark the device offline when it expires.");
  }
}

void FirebaseRestClient::handleUnoMessage(JsonDocument &message) {
  const String type = message["type"] | "";
  if (type == "STATE") {
    const String door = message["door"] | "";
    const String source = message["source"] | "";
    if (door == "locked" || door == "unlocked") {
      _doorStateKnown = true;
      _doorLocked = door == "locked";
      _doorStateDirty = true;
      _doorRetryAtMillis = millis();
      _doorRetryDelayMs = 1000;
      if (door == "unlocked" && source == "remote" &&
          !_activeCommandId.isEmpty()) {
        _activeRemoteStateSeen = true;
      }
    } else {
      Serial.println("Uno sent a STATE message with an invalid door value.");
    }
    return;
  }

  if (type == "EVENT") {
    const String event = message["event"] | "";
    const String source = message["source"] | "";
    const String actor = _deviceUid.isEmpty() ? String(FIREBASE_DEVICE_ID)
                                               : _deviceUid;
    if (event == "accessGranted" || event == "accessDenied" ||
        event == "securityLockout") {
      const String description =
          event == "accessGranted"
              ? "Arduino keypad access granted."
              : event == "securityLockout"
                    ? "Arduino keypad entered security lockout."
                    : "Arduino keypad access denied.";
      queueAudit(event, description, "local-keypad", actor);
    } else if (event == "automaticRelock") {
      const bool remote =
          !_lastRemoteCommandId.isEmpty() && source == "timer";
      const String commandId =
          remote ? _lastRemoteCommandId : "local-keypad";
      const String eventActor =
          remote ? _lastRemoteActorUid : actor;
      const String stableId =
          remote ? String(FIREBASE_DEVICE_ID) + "-cmd-" +
                       _lastRemoteCommandId + "-relock"
                 : String();
      if (queueAudit("automaticRelock",
                     remote ? "Arduino automatically relocked after remote access."
                            : "Arduino automatically relocked after local keypad access.",
                     commandId, eventActor, stableId) &&
          remote && _preferencesReady) {
        _preferences.remove("remoteId");
        _preferences.remove("remoteActor");
        _lastRemoteCommandId = "";
        _lastRemoteActorUid = "";
      }
    } else {
      Serial.printf("Arduino event was not mapped to the Firebase audit schema: %s\n",
                    event.c_str());
    }
    return;
  }

  if (type == "ack") {
    const String commandId = message["commandId"] | "";
    const String status = message["status"] | "";
    if (_activeCommandId.isEmpty() || commandId != _activeCommandId) {
      Serial.println("Ignored an acknowledgement for a command that is not active.");
      return;
    }
    if (status == "accepted") {
      _activeAckAccepted = true;
      return;
    }
    if (status == "executed") {
      if (!_activeAckAccepted || !_activeRemoteStateSeen) {
        failActiveCommand("Arduino execution acknowledgement did not match the expected UART sequence.");
        return;
      }
      _activePhase = "executed";
      if (_preferencesReady) _preferences.putString("activePhase", "executed");
      completeActiveCommand();
      return;
    }
    failActiveCommand("Arduino returned an unsupported acknowledgement status.");
    return;
  }

  if (type == "nack") {
    const String commandId = message["commandId"] | "";
    if (!_activeCommandId.isEmpty() && commandId == _activeCommandId) {
      const String reason = message["reason"] | "rejected";
      failActiveCommand(
          reason == "expired" ? "Arduino rejected an expired command."
                              : "Arduino rejected the command: " + reason);
    }
  }
}
