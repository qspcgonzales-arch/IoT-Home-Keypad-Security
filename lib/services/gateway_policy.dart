/// Which kind of gateway this app build is configured to command.
/// The choice is an explicit developer build setting; the app never switches
/// between modes on its own.
enum GatewayMode {
  simulated('simulated'),
  hardware('hardware');

  const GatewayMode(this.wireValue);

  /// Value the gateway publishes at `devices/gateway/mode`.
  final String wireValue;

  /// An omitted build define keeps local development in simulation mode.
  /// Invalid explicit values fail rather than silently selecting another mode.
  static GatewayMode parse(String? value) {
    if (value == null || value.isEmpty) return GatewayMode.simulated;
    return GatewayMode.values.firstWhere(
      (mode) => mode.wireValue == value,
      orElse: () => throw FormatException(
        'GATEWAY_MODE must be "simulated" or "hardware".',
      ),
    );
  }

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

  /// Only a fresh positive UART handshake proves the Uno is reachable.
  bool isArduinoHeartbeatFresh({
    required bool reportedConnected,
    required int? lastSeenMillis,
    required int nowMillis,
  }) => isHeartbeatFresh(
    reportedOnline: reportedConnected,
    lastSeenMillis: lastSeenMillis,
    nowMillis: nowMillis,
  );

  /// Command progress is trusted only when its device acknowledgement is
  /// present. A completed, failed, or expired command must carry a terminal
  /// acknowledgement timestamp.
  static bool hasDeviceAcknowledgement(Map<Object?, Object?> command) {
    final status = command['status'];
    final deviceId = command['deviceId'];
    final hasDevice = deviceId is String && deviceId.isNotEmpty;
    final acceptedAt = command['acceptedAt'];
    final acknowledgedAt = command['acknowledgedAt'];
    return switch (status) {
      'pending' => true,
      'accepted' =>
        hasDevice && acceptedAt is num && acceptedAt.isFinite && acceptedAt > 0,
      'completed' ||
      'failed' ||
      'expired' =>
        hasDevice &&
            acknowledgedAt is num &&
            acknowledgedAt.isFinite &&
            acknowledgedAt > 0,
      _ => false,
    };
  }
}
