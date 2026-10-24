import 'package:flutter_test/flutter_test.dart';
import 'package:smart_home_security/services/firebase_emulator_host.dart';

void main() {
  test('accepts private LAN IPv4 ranges', () {
    expect(validateFirebaseEmulatorHost('10.4.2.7'), isNull);
    expect(validateFirebaseEmulatorHost('172.16.0.8'), isNull);
    expect(validateFirebaseEmulatorHost('172.31.255.254'), isNull);
    expect(validateFirebaseEmulatorHost('192.168.1.25'), isNull);
  });

  test('rejects non-private, malformed, and non-IPv4 hosts', () {
    expect(
      validateFirebaseEmulatorHost('8.8.8.8'),
      contains('private LAN IPv4'),
    );
    expect(
      validateFirebaseEmulatorHost('127.0.0.1'),
      contains('private LAN IPv4'),
    );
    expect(
      validateFirebaseEmulatorHost('192.168.1.256'),
      contains('private LAN IPv4'),
    );
    expect(
      validateFirebaseEmulatorHost('test-pc.local'),
      contains('private LAN IPv4'),
    );
    expect(validateFirebaseEmulatorHost('  '), isNotNull);
  });
}
