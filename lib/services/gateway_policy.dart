/// Which kind of gateway this app build is configured to command.
/// The choice is an explicit developer build setting; the app never switches
/// between modes on its own.
enum GatewayMode {
  simulated('simulated'),
  hardware('hardware');

  const GatewayMode(this.wireValue);

  /// Value the gateway publishes at `devices/gateway/mode`.
  final String wireValue;

  /// Unknown or missing configuration fails closed to simulation.
  static GatewayMode parse(String? value) => GatewayMode.values.firstWhere(
    (mode) => mode.wireValue == value,
    orElse: () => GatewayMode.simulated,
  );

  /// `flutter run --dart-define=GATEWAY_MODE=hardware` opts in to hardware.
  static final GatewayMode configured = GatewayMode.parse(
    const String.fromEnvironment('GATEWAY_MODE', defaultValue: 'simulated'),
  );
}

/// Pure decisions about whether a reported gateway may be commanded.
/// Flutter cannot verify a device's identity; the database rules are the
/// authority for who may publish `mode: hardware`.
class GatewayPolicy {
  const GatewayPolicy({
    required this.expectedMode,
    this.heartbeatFreshness = const Duration(seconds: 30),
  });

  final GatewayMode expectedMode;
  final Duration heartbeatFreshness;

  bool modeMatches(String? reportedMode) =>
      reportedMode == expectedMode.wireValue;

  /// A heartbeat is fresh only when the gateway reports online AND its
  /// `lastSeen` is within the freshness window. Missing means OFFLINE.
  bool isHeartbeatFresh({
    required bool reportedOnline,
    required int? lastSeenMillis,
    required int nowMillis,
  }) {
    if (!reportedOnline || lastSeenMillis == null) return false;
    final age = nowMillis - lastSeenMillis;
    final window = heartbeatFreshness.inMilliseconds;
    return age >= -window && age <= window;
  }

  /// A completed/accepted command only counts as acknowledged when the device
  /// recorded who acknowledged it and when.
  static bool hasDeviceAcknowledgement(Map<Object?, Object?> command) {
    final status = command['status'];
    final deviceId = command['deviceId'];
    final hasDevice = deviceId is String && deviceId.isNotEmpty;
    return switch (status) {
      'accepted' => hasDevice && command['acceptedAt'] is num,
      'completed' => hasDevice && command['acknowledgedAt'] is num,
      _ => true,
    };
  }
}
