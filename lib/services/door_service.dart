import 'package:flutter/foundation.dart';

import '../models/security_event.dart';
import '../models/unlock_command.dart';

abstract class DoorService extends ChangeNotifier {
  bool get isLocked;
  bool get isOnline;
  bool get isPending;
  UnlockCommandStatus? get remoteUnlockStatus;
  bool get isLoading;
  bool get isDatabaseConnected;
  bool get isSimulated;
  bool get isHardware => false;
  bool get isArduinoReady => false;
  bool get isDeviceReady => isOnline && (!isHardware || isArduinoReady);
  bool get hasDoorStatus;
  bool get hasGatewayStatus;
  String? get feedback;
  List<SecurityEvent> get events;
  SecurityEvent? get latestEvent;
  bool get canSimulateConnectivity;
  bool get canRequestUnlock;
  int get unlockDurationSeconds;

  Future<bool> requestUnlock();
  Future<bool> setUnlockDurationSeconds(int seconds);
  void setOnline(bool online);
}
