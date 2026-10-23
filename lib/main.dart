import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/material.dart';

import 'firebase_options.dart';
import 'screens/login_screen.dart';
import 'screens/security_shell.dart';
import 'services/door_service.dart';
import 'services/demo_auth_service.dart';
import 'services/firebase_auth_service.dart';
import 'services/firebase_realtime_database_service.dart';
import 'services/mock_door_service.dart';
import 'theme/app_theme.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp(options: DefaultFirebaseOptions.currentPlatform);
  runApp(SmartHomeSecurityApp(authService: FirebaseAuthService()));
}

class SmartHomeSecurityApp extends StatefulWidget {
  const SmartHomeSecurityApp({super.key, required this.authService});

  final AuthService authService;

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
    _doorService = widget.authService is FirebaseAuthService && authorized
        ? FirebaseRealtimeDatabaseService()
        : MockDoorService();
    _signedInEmail = authorized ? email : null;
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
          )
        : SecurityShell(
            email: _signedInEmail!,
            doorService: _doorService,
            onLogout: _signOut,
          ),
  );
}
