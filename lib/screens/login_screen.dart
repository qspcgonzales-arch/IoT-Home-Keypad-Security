import 'package:flutter/material.dart';

import '../services/demo_auth_service.dart';
import '../services/firebase_auth_service.dart';
import '../theme/app_theme.dart';

class LoginScreen extends StatefulWidget {
  const LoginScreen({
    super.key,
    required this.authService,
    required this.onSignedIn,
    required this.onStartDemoMode,
  });

  final AuthService authService;
  final ValueChanged<String> onSignedIn;
  final VoidCallback onStartDemoMode;

  @override
  State<LoginScreen> createState() => _LoginScreenState();
}

class _LoginScreenState extends State<LoginScreen> {
  final _formKey = GlobalKey<FormState>();
  late final TextEditingController _emailController;
  late final TextEditingController _passwordController;
  bool _obscurePassword = true;
  bool _isLoading = false;
  String? _error;

  @override
  void initState() {
    super.initState();
    final isDemo = widget.authService is DemoAuthService;
    _emailController = TextEditingController(
      text: isDemo ? DemoAuthService.demoEmail : '',
    );
    _passwordController = TextEditingController(
      text: isDemo ? DemoAuthService.demoPassword : '',
    );
  }

  @override
  void dispose() {
    _emailController.dispose();
    _passwordController.dispose();
    super.dispose();
  }

  Future<void> _submit() async {
    if (!_formKey.currentState!.validate()) return;
    setState(() {
      _isLoading = true;
      _error = null;
    });
    await _authenticate(createAccount: false);
  }

  Future<void> _createAccount() async {
    if (!_formKey.currentState!.validate()) return;
    setState(() {
      _isLoading = true;
      _error = null;
    });
    await _authenticate(createAccount: true);
  }

  Future<void> _authenticate({required bool createAccount}) async {
    try {
      final authenticate = createAccount
          ? widget.authService.createAccount
          : widget.authService.signIn;
      final success = await authenticate(
        email: _emailController.text,
        password: _passwordController.text,
      );
      if (!mounted) return;
      if (success) {
        if (!widget.authService.isAuthorizedAdministrator) {
          await widget.authService.signOut();
          if (!mounted) return;
          setState(() {
            _isLoading = false;
            _error = 'This account is not authorized as an administrator.';
          });
          return;
        }
        setState(() => _isLoading = false);
        widget.onSignedIn(_emailController.text.trim());
      } else {
        setState(() {
          _isLoading = false;
          _error = 'Those demo credentials were not recognized.';
        });
      }
    } on AuthServiceException catch (error) {
      if (!mounted) return;
      setState(() {
        _isLoading = false;
        _error = error.message;
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    final isDemo = widget.authService is DemoAuthService;
    return Scaffold(
      body: SafeArea(
        child: Center(
          child: SingleChildScrollView(
            padding: const EdgeInsets.all(24),
            child: ConstrainedBox(
              constraints: const BoxConstraints(maxWidth: 440),
              child: Form(
                key: _formKey,
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    Container(
                      width: 72,
                      height: 72,
                      alignment: Alignment.center,
                      decoration: BoxDecoration(
                        color: AppTheme.navy,
                        borderRadius: BorderRadius.circular(22),
                      ),
                      child: const Icon(
                        Icons.shield_moon_outlined,
                        color: Colors.white,
                        size: 38,
                      ),
                    ),
                    const SizedBox(height: 28),
                    Text(
                      'Welcome home',
                      style: Theme.of(context).textTheme.headlineMedium
                          ?.copyWith(
                            color: AppTheme.navy,
                            fontWeight: FontWeight.w800,
                          ),
                    ),
                    const SizedBox(height: 8),
                    Text(
                      isDemo
                          ? 'LOCAL DEMO MODE — sign in with the demo credentials below.'
                          : 'FIREBASE AUTH — sign in with your authorized administrator account.',
                      style: TextStyle(color: AppTheme.muted),
                    ),
                    const SizedBox(height: 30),
                    TextFormField(
                      controller: _emailController,
                      keyboardType: TextInputType.emailAddress,
                      textInputAction: TextInputAction.next,
                      decoration: const InputDecoration(
                        labelText: 'Email address',
                        prefixIcon: Icon(Icons.mail_outline),
                      ),
                      validator: (value) {
                        final email = value?.trim() ?? '';
                        if (email.isEmpty) return 'Enter your email address';
                        if (!RegExp(
                          r'^[^@\s]+@[^@\s]+\.[^@\s]+$',
                        ).hasMatch(email)) {
                          return 'Enter a valid email address';
                        }
                        return null;
                      },
                    ),
                    const SizedBox(height: 16),
                    TextFormField(
                      controller: _passwordController,
                      obscureText: _obscurePassword,
                      textInputAction: TextInputAction.done,
                      onFieldSubmitted: (_) => _submit(),
                      decoration: InputDecoration(
                        labelText: 'Password',
                        prefixIcon: const Icon(Icons.lock_outline),
                        suffixIcon: IconButton(
                          tooltip: _obscurePassword
                              ? 'Show password'
                              : 'Hide password',
                          onPressed: () => setState(
                            () => _obscurePassword = !_obscurePassword,
                          ),
                          icon: Icon(
                            _obscurePassword
                                ? Icons.visibility_outlined
                                : Icons.visibility_off_outlined,
                          ),
                        ),
                      ),
                      validator: (value) => (value == null || value.length < 4)
                          ? 'Enter at least 4 characters'
                          : null,
                    ),
                    if (_error != null) ...[
                      const SizedBox(height: 12),
                      Text(
                        _error!,
                        style: const TextStyle(color: Color(0xFFB42318)),
                      ),
                    ],
                    const SizedBox(height: 24),
                    FilledButton(
                      onPressed: _isLoading ? null : _submit,
                      style: FilledButton.styleFrom(
                        backgroundColor: AppTheme.navy,
                        padding: const EdgeInsets.symmetric(vertical: 16),
                      ),
                      child: _isLoading
                          ? const SizedBox(
                              width: 20,
                              height: 20,
                              child: CircularProgressIndicator(
                                strokeWidth: 2,
                                color: Colors.white,
                              ),
                            )
                          : const Text('Sign in'),
                    ),
                    TextButton(
                      onPressed: _isLoading ? null : _createAccount,
                      child: const Text('Create account'),
                    ),
                    OutlinedButton.icon(
                      onPressed: _isLoading ? null : widget.onStartDemoMode,
                      icon: const Icon(Icons.play_circle_outline),
                      label: const Text('Try local demo mode'),
                    ),
                    const SizedBox(height: 20),
                    Container(
                      padding: const EdgeInsets.all(14),
                      decoration: BoxDecoration(
                        color: const Color(0xFFE9EEF5),
                        borderRadius: BorderRadius.circular(14),
                      ),
                      child: Row(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Icon(
                            Icons.info_outline,
                            color: AppTheme.navy,
                            size: 19,
                          ),
                          const SizedBox(width: 10),
                          Expanded(
                            child: Text(
                              isDemo
                                  ? 'LOCAL DEMO — simulated door data only. This mode cannot control Firebase devices.'
                                  : 'FIREBASE AUTH — access is limited to the configured administrator account.',
                              style: const TextStyle(
                                fontSize: 12,
                                color: AppTheme.navy,
                                height: 1.45,
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}
