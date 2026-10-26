/// Latest door and gateway values observed in Realtime Database.
class DoorStatus {
  const DoorStatus({
    required this.isLocked,
    required this.isOnline,
    this.gatewayLastSeenMillis,
    this.gatewayMode,
    this.irDigitalState,
    this.firmwareVersion,
  });

  const DoorStatus.initial()
    : isLocked = true,
      isOnline = false,
      gatewayLastSeenMillis = null,
      gatewayMode = null,
      irDigitalState = null,
      firmwareVersion = null;

  final bool isLocked;
  final bool isOnline;
  final int? gatewayLastSeenMillis;
  final String? gatewayMode;
  final String? irDigitalState;
  final String? firmwareVersion;

  DoorStatus copyWith({
    bool? isLocked,
    bool? isOnline,
    int? gatewayLastSeenMillis,
    String? gatewayMode,
    String? irDigitalState,
    String? firmwareVersion,
  }) => DoorStatus(
    isLocked: isLocked ?? this.isLocked,
    isOnline: isOnline ?? this.isOnline,
    gatewayLastSeenMillis: gatewayLastSeenMillis ?? this.gatewayLastSeenMillis,
    gatewayMode: gatewayMode ?? this.gatewayMode,
    irDigitalState: irDigitalState ?? this.irDigitalState,
    firmwareVersion: firmwareVersion ?? this.firmwareVersion,
  );
}
