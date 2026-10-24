import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:smart_home_security/main.dart';
import 'package:smart_home_security/models/security_event.dart';
import 'package:smart_home_security/screens/dashboard_screen.dart';
import 'package:smart_home_security/services/demo_auth_service.dart';
import 'package:smart_home_security/services/mock_door_service.dart';

void main() {
  testWidgets('demo sign in opens dashboard and audit navigation', (
    tester,
  ) async {
    await tester.pumpWidget(
      const SmartHomeSecurityApp(authService: DemoAuthService()),
    );

    expect(find.text('Welcome home'), findsOneWidget);
    await tester.enterText(
      find.byType(TextFormField).at(0),
      'demo@smarthome.local',
    );
    await tester.enterText(find.byType(TextFormField).at(1), 'demo1234');
    await tester.tap(find.text('Sign in'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 300));

    expect(find.text('Front door'), findsOneWidget);
    expect(find.text('LOCKED'), findsOneWidget);
    await tester.tap(find.text('Audit logs'));
    await tester.pumpAndSettle();
    expect(find.text('Security activity'), findsOneWidget);
    expect(find.text('Access granted'), findsOneWidget);
  });

  testWidgets('hardware-test demo starts locally without Firebase', (
    tester,
  ) async {
    await tester.pumpWidget(
      const SmartHomeSecurityApp(
        authService: DemoAuthService(),
        hardwareTestBuild: true,
        startInDemoMode: true,
      ),
    );

    expect(find.text('Front door'), findsOneWidget);
    expect(find.text('Welcome home'), findsNothing);
  });

  testWidgets('login validates email and password', (tester) async {
    await tester.pumpWidget(
      const SmartHomeSecurityApp(authService: DemoAuthService()),
    );
    await tester.enterText(find.byType(TextFormField).at(0), '');
    await tester.enterText(find.byType(TextFormField).at(1), '');
    await tester.tap(find.text('Sign in'));
    await tester.pumpAndSettle();
    expect(find.text('Enter your email address'), findsOneWidget);
    expect(find.text('Enter at least 4 characters'), findsOneWidget);
  });

  testWidgets(
    'confirmed remote unlock automatically relocks after four seconds',
    (tester) async {
      final service = MockDoorService();
      addTearDown(service.dispose);
      await tester.pumpWidget(
        MaterialApp(home: DashboardScreen(service: service)),
      );

      await tester.tap(find.text('Remote unlock'));
      await tester.pumpAndSettle();
      expect(find.text('Unlock the door?'), findsOneWidget);
      await tester.tap(find.text('Unlock'));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 450));
      expect(find.text('UNLOCKED'), findsAtLeastNWidgets(2));
      expect(service.events.first.type, SecurityEventType.remoteUnlock);

      await tester.pump(const Duration(seconds: 4));
      expect(find.text('LOCKED'), findsOneWidget);
      expect(service.events.first.type, SecurityEventType.automaticRelock);
    },
  );

  test('offline gateway reports a failed remote command', () async {
    final service = MockDoorService();
    addTearDown(service.dispose);
    service.setOnline(false);

    expect(await service.requestUnlock(), isFalse);
    expect(service.isLocked, isTrue);
    expect(service.feedback, contains('failed'));
    expect(service.events.first.type, SecurityEventType.accessDenied);
  });
}
