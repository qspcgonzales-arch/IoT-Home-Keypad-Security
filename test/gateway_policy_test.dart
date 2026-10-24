import 'package:flutter_test/flutter_test.dart';
import 'package:smart_home_security/services/gateway_policy.dart';

void main() {
  group('GatewayMode', () {
    test('defaults to simulated but rejects invalid explicit values', () {
      expect(GatewayMode.parse(null), GatewayMode.simulated);
      expect(() => GatewayMode.parse('HARDWARE'), throwsFormatException);
      expect(GatewayMode.parse('hardware'), GatewayMode.hardware);
      expect(GatewayMode.configured, GatewayMode.simulated);
    });
  });

  group('GatewayPolicy', () {
    const hw = GatewayPolicy(expectedMode: GatewayMode.hardware);
    const sim = GatewayPolicy(expectedMode: GatewayMode.simulated);

    test('mode must match exactly; no automatic switching', () {
      expect(hw.modeMatches('simulated'), isFalse);
      expect(hw.modeMatches(null), isFalse);
      expect(hw.modeMatches('hardware'), isTrue);
      expect(sim.modeMatches('hardware'), isFalse);
      expect(sim.modeMatches('simulated'), isTrue);
    });

    test('missing or stale heartbeat is offline', () {
      const now = 1000000;
      bool fresh(bool on, int? seen) => hw.isHeartbeatFresh(
        reportedOnline: on,
        lastSeenMillis: seen,
        nowMillis: now,
      );
      expect(fresh(true, now - 5000), isTrue);
      expect(fresh(true, null), isFalse);
      expect(fresh(false, now), isFalse);
      expect(fresh(true, now - 31000), isFalse);
      expect(fresh(true, now + 31000), isFalse);
    });

    test('Arduino readiness requires a fresh positive UART heartbeat', () {
      const now = 1000000;
      expect(
        hw.isArduinoHeartbeatFresh(
          reportedConnected: true,
          lastSeenMillis: now - 5000,
          nowMillis: now,
        ),
        isTrue,
      );
      expect(
        hw.isArduinoHeartbeatFresh(
          reportedConnected: false,
          lastSeenMillis: now,
          nowMillis: now,
        ),
        isFalse,
      );
      expect(
        hw.isArduinoHeartbeatFresh(
          reportedConnected: true,
          lastSeenMillis: now - 31000,
          nowMillis: now,
        ),
        isFalse,
      );
    });

    test('accepted/completed need device acknowledgement fields', () {
      expect(
        GatewayPolicy.hasDeviceAcknowledgement({'status': 'completed'}),
        isFalse,
      );
      expect(
        GatewayPolicy.hasDeviceAcknowledgement({
          'status': 'completed',
          'deviceId': 'esp32-frontdoor-1',
          'acknowledgedAt': 5,
        }),
        isTrue,
      );
      expect(
        GatewayPolicy.hasDeviceAcknowledgement({
          'status': 'failed',
          'deviceId': 'esp32-frontdoor-1',
          'acknowledgedAt': 5,
        }),
        isTrue,
      );
      expect(
        GatewayPolicy.hasDeviceAcknowledgement({
          'status': 'failed',
          'deviceId': 'esp32-frontdoor-1',
        }),
        isFalse,
      );
      expect(
        GatewayPolicy.hasDeviceAcknowledgement({'status': 'accepted'}),
        isFalse,
      );
      expect(
        GatewayPolicy.hasDeviceAcknowledgement({'status': 'pending'}),
        isTrue,
      );
    });
  });
}
