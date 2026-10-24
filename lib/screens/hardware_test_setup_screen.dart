import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../services/firebase_emulator_host.dart';

class HardwareTestSetupScreen extends StatefulWidget {
  const HardwareTestSetupScreen({
    super.key,
    required this.onConnectToEmulator,
    required this.onStartDemoMode,
  });

  final Future<void> Function(String host) onConnectToEmulator;
  final VoidCallback onStartDemoMode;

  @override
  State<HardwareTestSetupScreen> createState() =>
      _HardwareTestSetupScreenState();
}

class _HardwareTestSetupScreenState extends State<HardwareTestSetupScreen> {
  final _hostController = TextEditingController();
  bool _isStarting = false;
  bool _startupAttempted = false;
  String? _error;

  @override
  void dispose() {
    _hostController.dispose();
    super.dispose();
  }

  Future<void> _connectToEmulator() async {
    final host = _hostController.text.trim();
    final validationError = validateFirebaseEmulatorHost(host);
    if (validationError != null) {
      setState(() => _error = validationError);
      return;
    }

    setState(() {
      _startupAttempted = true;
      _isStarting = true;
      _error = null;
    });

    try {
      await widget.onConnectToEmulator(host);
      if (mounted) setState(() => _isStarting = false);
    } on FirebaseException catch (error) {
      _showStartupFailure(error.message ?? error.code);
    } on PlatformException catch (error) {
      _showStartupFailure(error.message ?? error.code);
    }
  }

  void _showStartupFailure(String message) {
    if (!mounted) return;
    setState(() {
      _isStarting = false;
      _error =
          'Firebase Emulator setup failed: $message. Force-close and reopen '
          'the app before trying another host. Production Firebase was not used.';
    });
  }

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: AppBar(title: const Text('Hardware Test Setup')),
    body: SafeArea(
      child: Center(
        child: ConstrainedBox(
          constraints: const BoxConstraints(maxWidth: 560),
          child: SingleChildScrollView(
            padding: const EdgeInsets.all(24),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Icon(
                  Icons.developer_board,
                  size: 48,
                  color: Color(0xFF17324D),
                ),
                const SizedBox(height: 16),
                Text(
                  'DEVELOPMENT ONLY — FIREBASE EMULATOR',
                  style: Theme.of(
                    context,
                  ).textTheme.titleLarge?.copyWith(fontWeight: FontWeight.w800),
                  textAlign: TextAlign.center,
                ),
                const SizedBox(height: 12),
                const Text(
                  'Enter the Windows test PC’s private LAN IPv4 address. '
                  'This build uses local-only Firebase options and routes both '
                  'Authentication and Realtime Database to that PC’s emulator. '
                  'It cannot fall back to production Firebase.',
                  textAlign: TextAlign.center,
                ),
                const SizedBox(height: 20),
                TextField(
                  key: const ValueKey('firebase-emulator-host'),
                  controller: _hostController,
                  enabled: !_startupAttempted && !_isStarting,
                  keyboardType: TextInputType.text,
                  autocorrect: false,
                  decoration: const InputDecoration(
                    labelText: 'Test PC LAN IPv4 address',
                    hintText: '192.168.1.25',
                    border: OutlineInputBorder(),
                    prefixIcon: Icon(Icons.computer),
                  ),
                ),
                const SizedBox(height: 8),
                const Text(
                  'The emulator must be running and reachable on TCP ports '
                  '9099 (Auth) and 9000 (Realtime Database). This address is '
                  'used only for this app launch and is not saved.',
                ),
                if (_error != null) ...[
                  const SizedBox(height: 16),
                  Text(
                    _error!,
                    key: const ValueKey('firebase-emulator-error'),
                    style: const TextStyle(color: Color(0xFFB42318)),
                  ),
                ],
                const SizedBox(height: 20),
                FilledButton(
                  key: const ValueKey('connect-firebase-emulator'),
                  onPressed: _startupAttempted || _isStarting
                      ? null
                      : _connectToEmulator,
                  child: _isStarting
                      ? const SizedBox(
                          width: 20,
                          height: 20,
                          child: CircularProgressIndicator(
                            strokeWidth: 2,
                            color: Colors.white,
                          ),
                        )
                      : const Text('Continue to hardware test'),
                ),
                const SizedBox(height: 12),
                OutlinedButton(
                  key: const ValueKey('start-local-demo'),
                  onPressed: _isStarting ? null : widget.onStartDemoMode,
                  child: const Text('Start local demo without Firebase'),
                ),
                if (_startupAttempted && !_isStarting) ...[
                  const SizedBox(height: 12),
                  const Text(
                    'Host changes require force-closing and reopening the app. '
                    'Emulator routing cannot be changed during a session.',
                    textAlign: TextAlign.center,
                  ),
                ],
              ],
            ),
          ),
        ),
      ),
    ),
  );
}
