import 'package:firebase_auth/firebase_auth.dart';

import 'demo_auth_service.dart';

class AuthServiceException implements Exception {
  const AuthServiceException(this.message);

  final String message;

  @override
  String toString() => message;
}

class FirebaseAuthService implements AuthService {
  /// UI gate paired with the same UID restriction in Realtime Database rules.
  static const administratorUid = 'ERxtj2dQuQRvv3Duw8Ud4DNMErD3';

  FirebaseAuthService({FirebaseAuth? auth})
    : _auth = auth ?? FirebaseAuth.instance;

  final FirebaseAuth _auth;

  @override
  String? get currentUserEmail => _auth.currentUser?.email;

  @override
  bool get isAuthorizedAdministrator =>
      _auth.currentUser?.uid == administratorUid;

  @override
  Future<bool> signIn({required String email, required String password}) async {
    try {
      await _auth.signInWithEmailAndPassword(
        email: email.trim(),
        password: password,
      );
      return true;
    } on FirebaseAuthException catch (error) {
      throw AuthServiceException(_messageFor(error.code));
    }
  }

  @override
  Future<bool> createAccount({
    required String email,
    required String password,
  }) async {
    try {
      await _auth.createUserWithEmailAndPassword(
        email: email.trim(),
        password: password,
      );
      return true;
    } on FirebaseAuthException catch (error) {
      throw AuthServiceException(_messageFor(error.code));
    }
  }

  @override
  Future<void> signOut() => _auth.signOut();

  String _messageFor(String code) => switch (code) {
    'invalid-email' => 'Enter a valid email address.',
    'user-disabled' => 'This account has been disabled.',
    'user-not-found' ||
    'wrong-password' ||
    'invalid-credential' => 'Email or password was not recognized.',
    'email-already-in-use' => 'An account already exists for this email.',
    'weak-password' => 'Choose a stronger password.',
    'operation-not-allowed' =>
      'Email and password sign-in is not enabled in Firebase Console.',
    'network-request-failed' =>
      'Could not reach Firebase. Check your connection.',
    _ => 'Firebase sign-in failed. Please try again.',
  };
}
