/// Latest door and gateway values observed in Realtime Database.
class DoorStatus {
  const DoorStatus({
    required this.isLocked,
    required this.isOnline,
    this.gatewayLastSeenMillis,
    this.gatewayMode,
  });

  const DoorStatus.initial()
    : isLocked = true,
      isOnline = false,
      gatewayLastSeenMillis = null,
      gatewayMode = null;

  final bool isLocked;
  final bool isOnline;
  final int? gatewayLastSeenMillis;
  final String? gatewayMode;

  DoorStatus copyWith({
    bool? isLocked,
    bool? isOnline,
    int? gatewayLastSeenMillis,
    String? gatewayMode,
  }) => DoorStatus(
    isLocked: isLocked ?? this.isLocked,
    isOnline: isOnline ?? this.isOnline,
    gatewayLastSeenMillis: gatewayLastSeenMillis ?? this.gatewayLastSeenMillis,
    gatewayMode: gatewayMode ?? this.gatewayMode,
  );
}
