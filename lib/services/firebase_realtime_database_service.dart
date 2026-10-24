import 'dart:async';

import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_database/firebase_database.dart';

import '../models/door_status.dart';
import '../models/security_event.dart';
import '../models/unlock_command.dart';
import 'door_service.dart';
import 'firebase_auth_service.dart';
import 'gateway_policy.dart';

/// Live database status and administrator command requests.
/// The app never writes a physical door state or a device acknowledgement.
class FirebaseRealtimeDatabaseService extends DoorService {
  FirebaseRealtimeDatabaseService({
    FirebaseDatabase? database,
    FirebaseAuth? auth,
    DateTime Function()? clock,
    GatewayMode? expectedMode,
  }) : _database = database ?? FirebaseDatabase.instance,
       _auth = auth ?? FirebaseAuth.instance,
       _clock = clock ?? DateTime.now,
       _policy = GatewayPolicy(
         expectedMode: expectedMode ?? GatewayMode.configured,
       ) {
    _doorSubscription = _database
        .ref('devices/frontDoor/isLocked')
        .onValue
        .listen(_onDoorValue, onError: _onDatabaseError);
    _gatewaySubscription = _database
        .ref('devices/gateway/isOnline')
        .onValue
        .listen(_onGatewayValue, onError: _onDatabaseError);
    _heartbeatSubscription = _database
        .ref('devices/gateway/lastSeen')
        .onValue
        .listen(_onHeartbeatValue, onError: _onDatabaseError);
    _modeSubscription = _database
        .ref('devices/gateway/mode')
        .onValue
        .listen(_onModeValue, onError: _onDatabaseError);
    _arduinoConnectedSubscription = _database
        .ref('devices/gateway/arduinoConnected')
        .onValue
        .listen(_onArduinoConnectedValue, onError: _onDatabaseError);
    _arduinoHeartbeatSubscription = _database
        .ref('devices/gateway/arduinoLastSeen')
        .onValue
        .listen(_onArduinoHeartbeatValue, onError: _onDatabaseError);
    _auditSubscription = newestAuditEvents().onValue.listen(
      _onAuditValue,
      onError: _onDatabaseError,
    );
    _connectionSubscription = _database
        .ref('.info/connected')
        .onValue
        .listen(_onConnectionValue, onError: _onDatabaseError);
    _unlockDurationSubscription = _database
        .ref('settings/frontDoor/unlockDurationSeconds')
        .onValue
        .listen(_onUnlockDurationValue, onError: _onDatabaseError);
    _heartbeatTimer = Timer.periodic(const Duration(seconds: 5), (_) {
      _clearAvailabilityFeedbackIfRecovered();
      notifyListeners();
    });
  }

  static const commandLifetime = Duration(seconds: 30);
  static const commandTimeoutGrace = Duration(seconds: 3);
  // The simulator writes every five seconds; allow transient Firebase/network
  // delays without briefly declaring a healthy gateway offline.
  static const heartbeatFreshness = Duration(seconds: 30);
  static const defaultUnlockDurationSeconds = 4;
  static const minUnlockDurationSeconds = 4;
  static const maxUnlockDurationSeconds = 30;

  final FirebaseDatabase _database;
  final FirebaseAuth _auth;
  final DateTime Function() _clock;
  final GatewayPolicy _policy;
  final List<SecurityEvent> _events = [];
  StreamSubscription<DatabaseEvent>? _doorSubscription;
  StreamSubscription<DatabaseEvent>? _gatewaySubscription;
  StreamSubscription<DatabaseEvent>? _heartbeatSubscription;
  StreamSubscription<DatabaseEvent>? _modeSubscription;
  StreamSubscription<DatabaseEvent>? _arduinoConnectedSubscription;
  StreamSubscription<DatabaseEvent>? _arduinoHeartbeatSubscription;
  StreamSubscription<DatabaseEvent>? _auditSubscription;
  StreamSubscription<DatabaseEvent>? _connectionSubscription;
  StreamSubscription<DatabaseEvent>? _unlockDurationSubscription;
  StreamSubscription<DatabaseEvent>? _commandSubscription;
  Timer? _heartbeatTimer;
  Timer? _commandExpiryTimer;
  DoorStatus _status = const DoorStatus.initial();
  bool _disposed = false;
  bool _doorLoaded = false;
  bool _gatewayLoaded = false;
  bool _heartbeatLoaded = false;
  bool _modeLoaded = false;
  bool _arduinoConnectedLoaded = false;
  bool _arduinoHeartbeatLoaded = false;
  bool _auditLoaded = false;
  bool _connectionKnown = false;
  bool _isDatabaseConnected = false;
  bool _arduinoConnected = false;
  int? _arduinoLastSeenMillis;
  bool _hasDoorStatus = false;
  bool _hasGatewayStatus = false;
  String? _feedback;
  String? _currentCommandId;
  UnlockCommandStatus? _remoteUnlockStatus;
  int _unlockDurationSeconds = defaultUnlockDurationSeconds;

  @override
  bool get isLocked => _status.isLocked;
  @override
  bool get isOnline => _policy.isHeartbeatFresh(
    reportedOnline: _status.isOnline,
    lastSeenMillis: _status.gatewayLastSeenMillis,
    nowMillis: _clock().millisecondsSinceEpoch,
  );

  @override
  bool get isPending =>
      _remoteUnlockStatus == UnlockCommandStatus.pending ||
      _remoteUnlockStatus == UnlockCommandStatus.accepted;
  @override
  UnlockCommandStatus? get remoteUnlockStatus => _remoteUnlockStatus;
  @override
  bool get isLoading =>
      !(_doorLoaded &&
          _gatewayLoaded &&
          _heartbeatLoaded &&
          _modeLoaded &&
          (_policy.expectedMode != GatewayMode.hardware ||
              (_arduinoConnectedLoaded && _arduinoHeartbeatLoaded)) &&
          _auditLoaded) &&
      _feedback == null &&
      !(_connectionKnown && !_isDatabaseConnected);
  @override
  bool get isDatabaseConnected => _isDatabaseConnected;
  @override
  bool get isSimulated => _status.gatewayMode == 'simulated';
  @override
  bool get hasDoorStatus => _hasDoorStatus;
  @override
  bool get hasGatewayStatus => _hasGatewayStatus;
  @override
  String? get feedback => _feedback;
  @override
  List<SecurityEvent> get events => List.unmodifiable(_events);
  @override
  SecurityEvent? get latestEvent => _events.isEmpty ? null : _events.first;
  @override
  bool get canSimulateConnectivity => false;
  @override
  bool get isHardware => _status.gatewayMode == GatewayMode.hardware.wireValue;
  @override
  bool get isArduinoReady => _policy.isArduinoHeartbeatFresh(
    reportedConnected: _arduinoConnected,
    lastSeenMillis: _arduinoLastSeenMillis,
    nowMillis: _clock().millisecondsSinceEpoch,
  );
  @override
  bool get canRequestUnlock =>
      _policy.modeMatches(_status.gatewayMode) &&
      _isDatabaseConnected &&
      isDeviceReady &&
      _hasDoorStatus &&
      _status.isLocked &&
      !isPending &&
      _remoteUnlockStatus != UnlockCommandStatus.unlocked;

  @override
  int get unlockDurationSeconds => _unlockDurationSeconds;

  @override
  Future<bool> setUnlockDurationSeconds(int seconds) async {
    if (seconds < minUnlockDurationSeconds ||
        seconds > maxUnlockDurationSeconds) {
      _feedback = 'Choose an unlock duration from 4 to 30 seconds.';
      notifyListeners();
      return false;
    }
    final user = _auth.currentUser;
    if (user == null || user.uid != FirebaseAuthService.administratorUid) {
      _feedback = 'Only an authorized administrator can change this setting.';
      notifyListeners();
      return false;
    }
    try {
      await _database
          .ref('settings/frontDoor/unlockDurationSeconds')
          .set(seconds);
      _unlockDurationSeconds = seconds;
      _feedback = 'Unlock duration saved: $seconds seconds.';
      notifyListeners();
      return true;
    } on FirebaseException catch (error) {
      _feedback =
          'Could not save unlock duration: ${error.message ?? error.code}';
      notifyListeners();
      return false;
    }
  }

  void _onUnlockDurationValue(DatabaseEvent event) {
    if (_disposed) return;
    final value = event.snapshot.value;
    if (value is num &&
        value.toInt() == value &&
        value >= minUnlockDurationSeconds &&
        value <= maxUnlockDurationSeconds) {
      _unlockDurationSeconds = value.toInt();
      notifyListeners();
    }
  }

  Query newestAuditEvents({int limit = 100}) =>
      _database.ref('auditLogs').orderByChild('timestamp').limitToLast(limit);

  @override
  Future<bool> requestUnlock() async {
    if (!canRequestUnlock) {
      _feedback = _unavailableReason;
      notifyListeners();
      return false;
    }
    final user = _auth.currentUser;
    if (user == null || user.uid != FirebaseAuthService.administratorUid) {
      _remoteUnlockStatus = UnlockCommandStatus.failed;
      _feedback = 'Command rejected. Administrator authentication required.';
      notifyListeners();
      return false;
    }

    final commandRef = _database.ref('commands').push();
    final commandId = commandRef.key;
    if (commandId == null) {
      _remoteUnlockStatus = UnlockCommandStatus.failed;
      _feedback = 'Command failed. Could not create a command ID.';
      notifyListeners();
      return false;
    }

    final expiresAt = _clock().add(commandLifetime).millisecondsSinceEpoch;
    final command = UnlockCommand(
      id: commandId,
      type: 'unlock',
      status: UnlockCommandStatus.pending,
      requestedBy: user.uid,
      createdAtMillis: _clock().millisecondsSinceEpoch,
      expiresAtMillis: expiresAt,
    );
    _currentCommandId = commandId;
    _remoteUnlockStatus = UnlockCommandStatus.pending;
    _feedback = 'COMMAND PENDING — waiting for device acknowledgement.';
    _commandSubscription?.cancel();
    _commandSubscription = commandRef.onValue.listen(
      _onCommandValue,
      onError: _onCommandError,
    );
    _commandExpiryTimer?.cancel();
    _commandExpiryTimer = Timer(
      commandLifetime + commandTimeoutGrace,
      _expireUnacknowledgedCommand,
    );
    notifyListeners();

    try {
      await commandRef.set(
        command.toDatabaseRecord(serverTimestamp: ServerValue.timestamp),
      );
      return true;
    } on FirebaseException catch (error) {
      _finishCommand(
        UnlockCommandStatus.failed,
        'Command failed: ${error.message ?? error.code}',
      );
      return false;
    }
  }

  String get _unavailableReason {
    if (!_isDatabaseConnected) return 'Command unavailable: database offline.';
    if (!_policy.modeMatches(_status.gatewayMode)) {
      return _policy.expectedMode == GatewayMode.hardware
          ? 'Command unavailable: no authenticated physical device is online.'
          : 'Command unavailable: no authenticated simulator is online.';
    }
    if (isHardware && !isArduinoReady) {
      return 'Command unavailable: Arduino UART has not been verified.';
    }
    if (!isOnline) return 'Command unavailable: device heartbeat is offline.';
    if (!_hasDoorStatus) return 'Command unavailable: door status is unknown.';
    if (!_status.isLocked) {
      return 'Command unavailable: door is already unlocked.';
    }
    return 'Command unavailable while another request is pending.';
  }

  void _onDoorValue(DatabaseEvent event) {
    if (_disposed) return;
    final value = event.snapshot.value;
    _doorLoaded = true;
    _hasDoorStatus = value is bool;
    if (value is bool) {
      final wasUnlocked = !_status.isLocked;
      _status = _status.copyWith(isLocked: value);
      _clearFailedCommandIfDoorIsUnlocked();
      if (value &&
          wasUnlocked &&
          _remoteUnlockStatus == UnlockCommandStatus.unlocked) {
        _remoteUnlockStatus = null;
        _currentCommandId = null;
        _commandSubscription?.cancel();
        _feedback = 'Device reported the door relocked.';
      }
    }
    _clearAvailabilityFeedbackIfRecovered();
    notifyListeners();
  }

  void _onGatewayValue(DatabaseEvent event) {
    if (_disposed) return;
    final value = event.snapshot.value;
    _gatewayLoaded = true;
    _hasGatewayStatus = value is bool;
    if (value is bool) _status = _status.copyWith(isOnline: value);
    _clearAvailabilityFeedbackIfRecovered();
    notifyListeners();
  }

  void _onHeartbeatValue(DatabaseEvent event) {
    if (_disposed) return;
    _heartbeatLoaded = true;
    final value = event.snapshot.value;
    if (value is num) {
      _status = _status.copyWith(gatewayLastSeenMillis: value.toInt());
    } else {
      _status = DoorStatus(
        isLocked: _status.isLocked,
        isOnline: _status.isOnline,
        gatewayMode: _status.gatewayMode,
      );
    }
    _clearAvailabilityFeedbackIfRecovered();
    notifyListeners();
  }

  void _onModeValue(DatabaseEvent event) {
    if (_disposed) return;
    _modeLoaded = true;
    final value = event.snapshot.value;
    _status = DoorStatus(
      isLocked: _status.isLocked,
      isOnline: _status.isOnline,
      gatewayLastSeenMillis: _status.gatewayLastSeenMillis,
      gatewayMode: value is String ? value : null,
    );
    _clearAvailabilityFeedbackIfRecovered();
    notifyListeners();
  }

  void _onArduinoConnectedValue(DatabaseEvent event) {
    if (_disposed) return;
    _arduinoConnectedLoaded = true;
    _arduinoConnected = event.snapshot.value == true;
    _clearAvailabilityFeedbackIfRecovered();
    notifyListeners();
  }

  void _onArduinoHeartbeatValue(DatabaseEvent event) {
    if (_disposed) return;
    _arduinoHeartbeatLoaded = true;
    final value = event.snapshot.value;
    _arduinoLastSeenMillis = value is num ? value.toInt() : null;
    _clearAvailabilityFeedbackIfRecovered();
    notifyListeners();
  }

  void _onAuditValue(DatabaseEvent event) {
    if (_disposed) return;
    _auditLoaded = true;
    final raw = event.snapshot.value;
    if (raw is! Map) {
      _events.clear();
      notifyListeners();
      return;
    }
    final parsed = <SecurityEvent>[];
    for (final row in raw.values) {
      if (row is! Map) continue;
      final timestamp = row['timestamp'];
      final description = row['description'];
      final type = _eventType(row['type']);
      if (timestamp is! num || description is! String || type == null) {
        continue;
      }
      parsed.add(
        SecurityEvent(
          type: type,
          timestamp: DateTime.fromMillisecondsSinceEpoch(timestamp.toInt()),
          description: description,
        ),
      );
    }
    parsed.sort((a, b) => b.timestamp.compareTo(a.timestamp));
    _events
      ..clear()
      ..addAll(parsed);
    notifyListeners();
  }

  void _onConnectionValue(DatabaseEvent event) {
    if (_disposed) return;
    _connectionKnown = true;
    _isDatabaseConnected = event.snapshot.value == true;
    _clearAvailabilityFeedbackIfRecovered();
    notifyListeners();
  }

  void _clearAvailabilityFeedbackIfRecovered() {
    if (_feedback?.startsWith('Command unavailable:') == true &&
        canRequestUnlock) {
      _feedback = null;
    }
  }

  void _onCommandValue(DatabaseEvent event) {
    if (_disposed || event.snapshot.key != _currentCommandId) return;
    final raw = event.snapshot.value;
    if (raw is! Map) return;
    final status = UnlockCommand.parseStatus(raw['status']);
    if (status == null || !GatewayPolicy.hasDeviceAcknowledgement(raw)) {
      _finishCommand(
        UnlockCommandStatus.failed,
        'Device returned an invalid command acknowledgement.',
      );
      return;
    }
    switch (status) {
      case UnlockCommandStatus.pending:
        _remoteUnlockStatus = status;
        _feedback = 'COMMAND PENDING — waiting for device acknowledgement.';
      case UnlockCommandStatus.accepted:
        _remoteUnlockStatus = status;
        _feedback = 'COMMAND PENDING — device accepted the request.';
      case UnlockCommandStatus.unlocked:
        _commandExpiryTimer?.cancel();
        _remoteUnlockStatus = status;
        _feedback = 'UNLOCKED — the device acknowledged the command.';
      case UnlockCommandStatus.failed:
        _finishCommand(
          status,
          raw['message'] is String
              ? raw['message'] as String
              : 'Device rejected the command.',
        );
      case UnlockCommandStatus.expired:
        _finishCommand(
          status,
          raw['message'] is String
              ? raw['message'] as String
              : 'Command expired before device acknowledgement.',
        );
    }
    notifyListeners();
  }

  void _onCommandError(Object error) {
    if (_disposed) return;
    _finishCommand(
      UnlockCommandStatus.failed,
      'Firebase command listener failed. Check connection and rules.',
    );
  }

  void _expireUnacknowledgedCommand() {
    if (_disposed || !isPending) return;
    _finishCommand(
      UnlockCommandStatus.expired,
      'EXPIRED — no device acknowledgement arrived in time.',
    );
  }

  void _finishCommand(UnlockCommandStatus status, String message) {
    _commandExpiryTimer?.cancel();
    _remoteUnlockStatus = status;
    _feedback = message;
    _commandSubscription?.cancel();
    _clearFailedCommandIfDoorIsUnlocked();
    notifyListeners();
  }

  /// A later live door-state update supersedes a stale failed/expired prompt.
  /// Keep the audit record intact; this only clears the Dashboard's transient
  /// command feedback and never changes the Firebase command record.
  void _clearFailedCommandIfDoorIsUnlocked() {
    if (_status.isLocked ||
        (_remoteUnlockStatus != UnlockCommandStatus.failed &&
            _remoteUnlockStatus != UnlockCommandStatus.expired)) {
      return;
    }
    _commandExpiryTimer?.cancel();
    _commandSubscription?.cancel();
    _remoteUnlockStatus = null;
    _currentCommandId = null;
    _feedback = null;
  }

  SecurityEventType? _eventType(Object? value) => switch (value) {
    'accessGranted' => SecurityEventType.accessGranted,
    'accessDenied' => SecurityEventType.accessDenied,
    'remoteUnlock' => SecurityEventType.remoteUnlock,
    'automaticRelock' => SecurityEventType.automaticRelock,
    'securityLockout' => SecurityEventType.securityLockout,
    'commandFailed' => SecurityEventType.commandFailed,
    'commandExpired' => SecurityEventType.commandExpired,
    _ => null,
  };

  void _onDatabaseError(Object error) {
    if (_disposed) return;
    _feedback = 'Database connection failed. Check your connection and rules.';
    notifyListeners();
  }

  @override
  void setOnline(bool online) {
    // Real device connectivity is reported by the simulator/device heartbeat.
  }

  @override
  void dispose() {
    _disposed = true;
    _heartbeatTimer?.cancel();
    _commandExpiryTimer?.cancel();
    _doorSubscription?.cancel();
    _gatewaySubscription?.cancel();
    _heartbeatSubscription?.cancel();
    _modeSubscription?.cancel();
    _arduinoConnectedSubscription?.cancel();
    _arduinoHeartbeatSubscription?.cancel();
    _auditSubscription?.cancel();
    _connectionSubscription?.cancel();
    _unlockDurationSubscription?.cancel();
    _commandSubscription?.cancel();
    super.dispose();
  }
}
