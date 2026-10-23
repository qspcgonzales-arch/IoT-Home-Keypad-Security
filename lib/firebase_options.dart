import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/foundation.dart';

/// Firebase app configuration for the registered Android app.
/// Add other platform options after registering those apps in Firebase.
class DefaultFirebaseOptions {
  static FirebaseOptions get currentPlatform {
    if (defaultTargetPlatform == TargetPlatform.android) return android;
    throw UnsupportedError(
      'Firebase is configured for Android only. Register this platform in Firebase first.',
    );
  }

  static const FirebaseOptions android = FirebaseOptions(
    apiKey: 'AIzaSyBNxF6nC2_gta_Ux_49qRIrMLxzOwNXPW8',
    appId: '1:737687406041:android:f22f9bf4f62f315f8605c8',
    messagingSenderId: '737687406041',
    projectId: 'smart-home-security-tip',
    databaseURL:
        'https://smart-home-security-tip-default-rtdb.asia-southeast1.firebasedatabase.app',
    storageBucket: 'smart-home-security-tip.firebasestorage.app',
  );
}
