abstract interface class AuthService {
  String? get currentUserEmail;
  bool get isAuthorizedAdministrator;

  Future<bool> signIn({required String email, required String password});

  Future<bool> createAccount({required String email, required String password});

  Future<void> signOut();
}

class DemoAuthService implements AuthService {
  const DemoAuthService();

  // Local-only demo authentication; the production app uses Firebase Auth.
  static const demoEmail = 'demo@smarthome.local';
  static const demoPassword = 'demo1234';

  @override
  String? get currentUserEmail => null;

  @override
  bool get isAuthorizedAdministrator => true;

  @override
  Future<bool> signIn({required String email, required String password}) async {
    await Future<void>.delayed(const Duration(milliseconds: 250));
    return email.trim().toLowerCase() == demoEmail && password == demoPassword;
  }

  @override
  Future<bool> createAccount({
    required String email,
    required String password,
  }) => signIn(email: email, password: password);

  @override
  Future<void> signOut() async {}
}
