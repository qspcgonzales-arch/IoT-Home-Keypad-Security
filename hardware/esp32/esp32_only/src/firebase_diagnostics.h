#pragma once

#include <Arduino.h>

class FirebaseDiagnostics {
 public:
  ~FirebaseDiagnostics();
  void begin();
  void tick(bool doorLocked, bool sensorHigh);
  bool recordEvent(const String &type, const String &description,
                   const String &commandId = "local-keypad",
                   const String &requestedBy = String());

 private:
  bool isConfigured() const;
  bool ensureAuth();
  bool signIn();
  bool refreshAuth();
  bool ensureDeviceRegistration();
  bool publishStatus(bool doorLocked, bool sensorHigh);
  bool processPendingCommand();
  bool queueAudit(const String &type, const String &description,
                  const String &commandId, const String &requestedBy,
                  const String &stableId = String());
  bool drainAuditQueue();
  bool databaseGet(const String &path, const String &query, String &response);
  bool databasePatch(const String &path, const String &payload);
  bool databasePut(const String &path, const String &payload);
  int databaseRequest(const String &method, const String &path,
                      const String &query, const String &payload,
                      String &response);
  int sendRequest(const String &method, const String &url,
                  const String &payload, String &response,
                  const char *contentType = "application/json");
  String authUrl(const String &endpoint) const;
  String tokenUrl() const;
  String databaseUrl(const String &path, const String &query) const;
  String encodeUrl(const String &value) const;
  void maintainWifi();
  void scheduleAuthRetry();
  void scheduleDatabaseRetry();
  static bool validIdentifier(const String &value, size_t maxLength);
  static bool clockIsValid();
  static uint64_t nowEpochMs();
  static bool deadlineReached(uint32_t now, uint32_t deadline);

  class State;
  State *_state = nullptr;
};
