import 'package:flutter/material.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:smart_home_security/screens/hardware_test_setup_screen.dart';

void main() {
  testWidgets('routes the entered host to emulator setup', (tester) async {
    String? configuredHost;
    await tester.pumpWidget(
      MaterialApp(
        home: HardwareTestSetupScreen(
          onConnectToEmulator: (host) async => configuredHost = host,
          onStartDemoMode: () {},
        ),
      ),
    );

    await tester.enterText(
      find.byKey(const ValueKey('firebase-emulator-host')),
      '192.168.1.25',
    );
    await tester.tap(find.byKey(const ValueKey('connect-firebase-emulator')));
    await tester.pumpAndSettle();

    expect(configuredHost, '192.168.1.25');
    expect(find.textContaining('Host changes require'), findsOneWidget);
  });

  testWidgets('rejects a public host before emulator initialization', (
    tester,
  ) async {
    var initializationAttempted = false;
    await tester.pumpWidget(
      MaterialApp(
        home: HardwareTestSetupScreen(
          onConnectToEmulator: (_) async => initializationAttempted = true,
          onStartDemoMode: () {},
        ),
      ),
    );

    await tester.enterText(
      find.byKey(const ValueKey('firebase-emulator-host')),
      '8.8.8.8',
    );
    await tester.tap(find.byKey(const ValueKey('connect-firebase-emulator')));
    await tester.pumpAndSettle();

    expect(initializationAttempted, isFalse);
    expect(
      find.byKey(const ValueKey('firebase-emulator-error')),
      findsOneWidget,
    );
  });

  testWidgets('does not allow host changes after startup fails', (
    tester,
  ) async {
    await tester.pumpWidget(
      MaterialApp(
        home: HardwareTestSetupScreen(
          onConnectToEmulator: (_) async {
            throw FirebaseException(
              plugin: 'firebase_core',
              message: 'Emulator is unreachable',
            );
          },
          onStartDemoMode: () {},
        ),
      ),
    );

    await tester.enterText(
      find.byKey(const ValueKey('firebase-emulator-host')),
      '192.168.1.25',
    );
    await tester.tap(find.byKey(const ValueKey('connect-firebase-emulator')));
    await tester.pumpAndSettle();

    expect(find.textContaining('Force-close and reopen'), findsOneWidget);
    expect(
      tester
          .widget<TextField>(
            find.byKey(const ValueKey('firebase-emulator-host')),
          )
          .enabled,
      isFalse,
    );
  });

  testWidgets('local demo is available without entering an emulator host', (
    tester,
  ) async {
    var demoStarted = false;
    await tester.pumpWidget(
      MaterialApp(
        home: HardwareTestSetupScreen(
          onConnectToEmulator: (_) async {},
          onStartDemoMode: () => demoStarted = true,
        ),
      ),
    );

    final demoButton = find.byKey(const ValueKey('start-local-demo'));
    await tester.ensureVisible(demoButton);
    await tester.tap(demoButton);

    expect(demoStarted, isTrue);
  });
}
