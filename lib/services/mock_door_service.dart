import 'dart:async';

import '../models/security_event.dart';
import '../models/unlock_command.dart';
import 'door_service.dart';

/// Local simulator used by demonstration mode and widget tests.
class MockDoorService extends DoorService {
  MockDoorService({DateTime Function()? clock})
    : _clock = clock ?? DateTime.now {
    _addEvent(SecurityEventType.accessGranted, 'Demo session started');
  }

  final DateTime Function() _clock;
  final List<SecurityEvent> _events = [];
  Timer? _relockTimer;
  bool _isLocked = true;
  bool _isOnline = true;
  bool _isPending = false;
  int _unlockDurationSeconds = 4;
  UnlockCommandStatus? _remoteUnlockStatus;
  String? _feedback;

  @override
  bool get isLocked => _isLocked;
  @override
  bool get isOnline => _isOnline;
  @override
  bool get isPending => _isPending;
  @override
  UnlockCommandStatus? get remoteUnlockStatus => _remoteUnlockStatus;
  @override
  bool get isLoading => false;
  @override
  bool get isDatabaseConnected => true;
  @override
  bool get isSimulated => true;
  @override
  bool get hasDoorStatus => true;
  @override
  bool get hasGatewayStatus => true;
  @override
  String? get feedback => _feedback;
  @override
  List<SecurityEvent> get events => List.unmodifiable(_events);
  @override
  SecurityEvent? get latestEvent => _events.isEmpty ? null : _events.first;
  @override
  bool get canSimulateConnectivity => true;
  @override
  bool get canRequestUnlock => true;
  @override
  int get unlockDurationSeconds => _unlockDurationSeconds;

  @override
  Future<bool> setUnlockDurationSeconds(int seconds) async {
    if (seconds < 4 || seconds > 30) return false;
    _unlockDurationSeconds = seconds;
    _feedback = 'Unlock duration set to $seconds seconds.';
    notifyListeners();
    return true;
  }

  @override
  Future<bool> requestUnlock() async {
    if (_isPending || !_isLocked) return false;
    _isPending = true;
    _remoteUnlockStatus = UnlockCommandStatus.pending;
    _feedback = 'Command pending…';
    notifyListeners();
    await Future<void>.delayed(const Duration(milliseconds: 450));
    if (!_isOnline) {
      _isPending = false;
      _remoteUnlockStatus = UnlockCommandStatus.failed;
      _feedback = 'Command failed. Device is offline.';
      _addEvent(
        SecurityEventType.accessDenied,
        'Remote command failed: device offline',
      );
      notifyListeners();
      return false;
    }
    _isPending = false;
    _remoteUnlockStatus = UnlockCommandStatus.unlocked;
    _isLocked = false;
    _feedback =
        'Door unlocked. It will relock in $_unlockDurationSeconds seconds.';
    _addEvent(
      SecurityEventType.remoteUnlock,
      'Remote unlock command completed',
    );
    _relockTimer?.cancel();
    _relockTimer = Timer(Duration(seconds: _unlockDurationSeconds), _relock);
    notifyListeners();
    return true;
  }

  @override
  void setOnline(bool online) {
    if (_isOnline == online) return;
    _isOnline = online;
    if (!online) {
      _addEvent(SecurityEventType.securityLockout, 'Gateway connection lost');
    }
    _feedback = online
        ? 'Gateway connected.'
        : 'Gateway offline. Commands are unavailable.';
    notifyListeners();
  }

  void recordDeniedAccess() {
    _addEvent(SecurityEventType.accessDenied, 'Invalid keypad code entered');
    _feedback = 'Access denied. Invalid keypad code.';
    notifyListeners();
  }

  void _relock() {
    _isLocked = true;
    _remoteUnlockStatus = null;
    _feedback = 'Door automatically relocked.';
    _addEvent(
      SecurityEventType.automaticRelock,
      'Door relocked after timed access',
    );
    notifyListeners();
  }

  void _addEvent(SecurityEventType type, String description) {
    _events.insert(
      0,
      SecurityEvent(type: type, timestamp: _clock(), description: description),
    );
  }

  @override
  void dispose() {
    _relockTimer?.cancel();
    super.dispose();
  }
}
