#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

struct CloudCommand {
  String id;
  String requestedBy;
  uint64_t expiresAtEpochMs = 0;
  uint32_t unlockDurationMs = 4000;
};

class FirebaseRestClient {
 public:
  void begin();
  void tick(bool arduinoReady, bool doorStateKnown, bool doorLocked,
            uint64_t arduinoLastSeenEpochMs);
  bool readyForCommands();
  bool beginNextCommand(CloudCommand &command);
  void noteCommandSent();
  void handleUnoMessage(JsonDocument &message);
  void failActiveCommand(const String &reason);

 private:
  bool isConfigured() const;
  bool ensureAuth();
  bool signIn();
  bool refreshAuth();
  bool ensureDeviceRegistration();
  bool recoverActiveCommand();
  bool readUnlockDuration(uint32_t &durationMs);
  bool readCommand(const String &commandId, JsonDocument &record);
  bool writeCommandPatch(const String &commandId, JsonDocument &patch);
  bool rejectPendingCommand(const CloudCommand &command, const String &status,
                            const String &message);
  bool completeActiveCommand();
  bool applyActiveFailure();
  bool persistActive(const String &commandId, const String &phase);
  void clearActive();
  void updateDoorState();
  void updateGatewayHeartbeat(bool arduinoReady,
                              uint64_t arduinoLastSeenEpochMs);
  bool queueAudit(const String &type, const String &description,
                  const String &commandId, const String &actorUid,
                  const String &stableAuditId = String());
  void drainAuditQueue();
  bool databaseGet(const String &path, const String &query, String &response);
  bool databasePatch(const String &path, const JsonDocument &document);
  bool databasePut(const String &path, const String &payload,
                   const char *headerName = nullptr,
                   const char *headerValue = nullptr);
  int databaseRequest(const String &method, const String &path,
                      const String &query, const String &payload,
                      String &response, const char *headerName = nullptr,
                      const char *headerValue = nullptr);
  int sendRequest(const String &method, const String &url,
                  const String &payload, String &response,
                  const char *contentType = "application/json",
                  const char *headerName = nullptr,
                  const char *headerValue = nullptr);
  String databaseUrl(const String &path, const String &query) const;
  String authUrl(const String &endpoint) const;
  String tokenUrl() const;
  String encodeUrl(const String &value) const;
  void scheduleAuthRetry();
  void scheduleDatabaseRetry(uint32_t &deadline, uint32_t &delayMs);
  static bool validIdentifier(const String &value, size_t maxLength);
  static bool clockIsValid();
  static uint64_t nowEpochMs();
  static bool deadlineReached(uint32_t now, uint32_t deadline);

  Preferences _preferences;
  String _idToken;
  String _refreshToken;
  String _deviceUid;
  String _activeCommandId;
  String _activeActorUid;
  String _activePhase;
  String _activeFailureMessage;
  String _lastRemoteCommandId;
  String _lastRemoteActorUid;
  uint64_t _activeExpiresAtEpochMs = 0;
  uint32_t _activeStartedAtMillis = 0;
  uint32_t _tokenRefreshAtMillis = 0;
  uint32_t _authRetryAtMillis = 0;
  uint32_t _authRetryDelayMs = 5000;
  uint32_t _registrationCheckAtMillis = 0;
  uint32_t _heartbeatAtMillis = 0;
  uint32_t _commandPollAtMillis = 0;
  uint32_t _doorRetryAtMillis = 0;
  uint32_t _doorRetryDelayMs = 1000;
  uint32_t _auditRetryAtMillis = 0;
  uint32_t _auditRetryDelayMs = 1000;
  bool _preferencesReady = false;
  bool _authenticated = false;
  bool _registered = false;
  bool _recoveryPending = false;
  bool _activeAckAccepted = false;
  bool _activeRemoteStateSeen = false;
  bool _doorStateKnown = false;
  bool _doorLocked = true;
  bool _doorStateDirty = false;
};
