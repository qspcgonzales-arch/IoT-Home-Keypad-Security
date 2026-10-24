import 'package:firebase_auth/firebase_auth.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:flutter/material.dart';

import 'firebase_options.dart';
import 'screens/hardware_test_setup_screen.dart';
import 'screens/login_screen.dart';
import 'screens/security_shell.dart';
import 'services/door_service.dart';
import 'services/demo_auth_service.dart';
import 'services/firebase_emulator_host.dart';
import 'services/firebase_auth_service.dart';
import 'services/firebase_realtime_database_service.dart';
import 'services/mock_door_service.dart';
import 'theme/app_theme.dart';

const _hardwareTestBuild = bool.fromEnvironment('HARDWARE_TEST_BUILD');
const _configuredGatewayMode = String.fromEnvironment(
  'GATEWAY_MODE',
  defaultValue: 'simulated',
);
const _firebaseEmulatorProjectId = 'demo-smart-home';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  if (_hardwareTestBuild) {
    if (_configuredGatewayMode != 'hardware') {
      runApp(const _HardwareTestConfigurationRequiredApp());
      return;
    }
    runApp(
      HardwareTestSetupScreen(
        onConnectToEmulator: _connectToFirebaseEmulator,
        onStartDemoMode: _startHardwareTestDemo,
      ),
    );
    return;
  }

  await Firebase.initializeApp(options: DefaultFirebaseOptions.currentPlatform);
  runApp(SmartHomeSecurityApp(authService: FirebaseAuthService()));
}

Future<void> _connectToFirebaseEmulator(String host) async {
  final validationError = validateFirebaseEmulatorHost(host);
  if (validationError != null) {
    throw ArgumentError.value(host, 'host', validationError);
  }

  await Firebase.initializeApp(
    options: const FirebaseOptions(
      apiKey: 'fake-api-key',
      appId: '1:1234567890:android:emulator',
      messagingSenderId: '1234567890',
      projectId: _firebaseEmulatorProjectId,
      databaseURL:
          'https://$_firebaseEmulatorProjectId-default-rtdb.firebaseio.com',
    ),
  );
  FirebaseAuth.instance.useAuthEmulator(host, 9099);
  FirebaseDatabase.instance.useDatabaseEmulator(host, 9000);
  runApp(
    SmartHomeSecurityApp(
      authService: FirebaseAuthService(),
      hardwareTestBuild: true,
    ),
  );
}

void _startHardwareTestDemo() {
  runApp(
    const SmartHomeSecurityApp(
      authService: DemoAuthService(),
      hardwareTestBuild: true,
      startInDemoMode: true,
    ),
  );
}

class _HardwareTestConfigurationRequiredApp extends StatelessWidget {
  const _HardwareTestConfigurationRequiredApp();

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'Hardware Test Configuration Required',
    home: Scaffold(
      body: SafeArea(
        child: Center(
          child: Padding(
            padding: const EdgeInsets.all(24),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                const Icon(Icons.cloud_off, size: 48, color: Colors.red),
                const SizedBox(height: 16),
                Text(
                  'HARDWARE TEST BUILD BLOCKED',
                  style: Theme.of(context).textTheme.titleLarge,
                  textAlign: TextAlign.center,
                ),
                const SizedBox(height: 12),
                const Text(
                  'This development build must be built with '
                  'GATEWAY_MODE=hardware. Firebase was not initialized; '
                  'production services will not be used.',
                  textAlign: TextAlign.center,
                ),
              ],
            ),
          ),
        ),
      ),
    ),
  );
}

class SmartHomeSecurityApp extends StatefulWidget {
  const SmartHomeSecurityApp({
    super.key,
    required this.authService,
    this.hardwareTestBuild = false,
    this.startInDemoMode = false,
  });

  final AuthService authService;
  final bool hardwareTestBuild;
  final bool startInDemoMode;

  @override
  State<SmartHomeSecurityApp> createState() => _SmartHomeSecurityAppState();
}

class _SmartHomeSecurityAppState extends State<SmartHomeSecurityApp> {
  late DoorService _doorService;
  String? _signedInEmail;
  bool _demoMode = false;

  @override
  void initState() {
    super.initState();
    final email = widget.authService.currentUserEmail;
    final authorized = widget.authService.isAuthorizedAdministrator;
    _demoMode = widget.startInDemoMode;
    _doorService =
        !_demoMode && widget.authService is FirebaseAuthService && authorized
        ? FirebaseRealtimeDatabaseService()
        : MockDoorService();
    _signedInEmail = _demoMode
        ? DemoAuthService.demoEmail
        : authorized
        ? email
        : null;
    if (email != null && !authorized) widget.authService.signOut();
  }

  @override
  void dispose() {
    _doorService.dispose();
    super.dispose();
  }

  void _signIn(String email) {
    if (!_demoMode &&
        widget.authService is FirebaseAuthService &&
        widget.authService.isAuthorizedAdministrator) {
      _doorService.dispose();
      _doorService = FirebaseRealtimeDatabaseService();
    }
    setState(() => _signedInEmail = email);
  }

  void _startDemoMode() {
    _doorService.dispose();
    _doorService = MockDoorService();
    setState(() {
      _demoMode = true;
      _signedInEmail = DemoAuthService.demoEmail;
    });
  }

  Future<void> _signOut() async {
    if (!_demoMode) await widget.authService.signOut();
    if (mounted) {
      _doorService.dispose();
      _doorService = MockDoorService();
      setState(() {
        _demoMode = false;
        _signedInEmail = null;
      });
    }
  }

  @override
  Widget build(BuildContext context) => MaterialApp(
    title: 'Keypad Security Terminal',
    debugShowCheckedModeBanner: false,
    theme: AppTheme.light,
    home: _signedInEmail == null
        ? LoginScreen(
            authService: widget.authService,
            onSignedIn: _signIn,
            onStartDemoMode: _startDemoMode,
            hardwareTestBuild: widget.hardwareTestBuild,
          )
        : SecurityShell(
            email: _signedInEmail!,
            doorService: _doorService,
            onLogout: _signOut,
          ),
  );
}
