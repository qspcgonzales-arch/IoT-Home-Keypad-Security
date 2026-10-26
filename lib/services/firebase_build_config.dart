import 'package:firebase_core/firebase_core.dart';
import 'package:flutter/services.dart' show appFlavor;

import '../firebase_options.dart';

class FirebaseBuildConfig {
  const FirebaseBuildConfig({
    required this.environment,
    required this.flavor,
    required this.projectId,
    required this.apiKey,
    required this.appId,
    required this.messagingSenderId,
    required this.databaseUrl,
    required this.administratorUid,
    required this.deviceId,
    required this.gatewayMode,
  });

  static const _declaredEnvironment = String.fromEnvironment('FIREBASE_ENV');
  static const _buildFlavor = appFlavor;
  static const _buildEnvironment = _buildFlavor ??
      (_declaredEnvironment == '' ? 'production' : _declaredEnvironment);
  static const stagingProjectId = String.fromEnvironment(
    'FIREBASE_PROJECT_ID',
  );
  static const stagingApiKey = String.fromEnvironment('FIREBASE_API_KEY');
  static const stagingAppId = String.fromEnvironment('FIREBASE_APP_ID');
  static const stagingMessagingSenderId = String.fromEnvironment(
    'FIREBASE_MESSAGING_SENDER_ID',
  );
  static const stagingDatabaseUrl = String.fromEnvironment(
    'FIREBASE_DATABASE_URL',
  );
  static const stagingAdministratorUid = String.fromEnvironment(
    'FIREBASE_ADMIN_UID',
  );
  static const stagingDeviceId = String.fromEnvironment('FIREBASE_DEVICE_ID');
  static const _buildGatewayMode = String.fromEnvironment(
    'GATEWAY_MODE',
    defaultValue: 'simulated',
  );

  static const confirmedStagingProjectId = 'iothomekeypadsecurity-staging';
  static const confirmedStagingProjectNumber = '637464771451';
  static const confirmedStagingDeviceId = 'esp32-frontdoor-1';
  static const confirmedStagingDatabaseUrl =
      'https://iothomekeypadsecurity-staging-default-rtdb.asia-southeast1.firebasedatabase.app';
  static const _productionAdministratorUid =
      'ERxtj2dQuQRvv3Duw8Ud4DNMErD3';

  static const current = FirebaseBuildConfig(
    environment: _buildEnvironment,
    flavor: _buildFlavor,
    projectId: stagingProjectId,
    apiKey: stagingApiKey,
    appId: stagingAppId,
    messagingSenderId: stagingMessagingSenderId,
    databaseUrl: stagingDatabaseUrl,
    administratorUid: stagingAdministratorUid,
    deviceId: stagingDeviceId,
    gatewayMode: _buildGatewayMode,
  );

  final String environment;
  final String? flavor;
  final String projectId;
  final String apiKey;
  final String appId;
  final String messagingSenderId;
  final String databaseUrl;
  final String administratorUid;
  final String deviceId;
  final String gatewayMode;

  bool get isStaging => environment == 'staging';
  bool get isRemoteHardwareTest => isStaging;
  String get effectiveAdministratorUid => isStaging
      ? administratorUid
      : 'ERxtj2dQuQRvv3Duw8Ud4DNMErD3';
  String? get effectiveDeviceId => isStaging ? deviceId : null;

  String? get validationError {
    if (_declaredEnvironment.isNotEmpty &&
        _declaredEnvironment != environment) {
      return 'FIREBASE_ENV does not match the Android build flavor.';
    }
    if (environment != 'production' && environment != 'staging') {
      return 'FIREBASE_ENV must be "production" or "staging".';
    }
    if (!isStaging) return null;
    if (flavor != 'staging') {
      return 'Build the staging APK with the Android "staging" flavor.';
    }
    if (projectId != confirmedStagingProjectId) {
      return 'Staging must use the confirmed iothomekeypadsecurity-staging project.';
    }
    if (apiKey.isEmpty ||
        appId.isEmpty ||
        messagingSenderId.isEmpty ||
        databaseUrl.isEmpty) {
      return 'Staging Firebase options are incomplete. Copy the Android app '
          'values from the staging Firebase Console.';
    }
    if (apiKey.contains('REPLACE') ||
        appId.contains('REPLACE') ||
        messagingSenderId.contains('REPLACE') ||
        databaseUrl.contains('REPLACE')) {
      return 'Replace the staging Firebase example values with the actual '
          'Android app configuration.';
    }
    if (messagingSenderId != confirmedStagingProjectNumber) {
      return 'The staging sender ID does not match the confirmed Firebase project.';
    }
    final appIdParts = appId.split(':');
    if (appIdParts.length != 4 ||
        appIdParts[0] != '1' ||
        appIdParts[1] != messagingSenderId ||
        appIdParts[2] != 'android' ||
        appIdParts[3].isEmpty) {
      return 'The staging Android app ID and sender ID do not match.';
    }
    final parsedDatabaseUrl = Uri.tryParse(databaseUrl);
    final databaseHost = parsedDatabaseUrl?.host ?? '';
    final databaseMatchesProject =
        databaseHost.startsWith('$projectId-default-rtdb.') ||
        databaseHost == '$projectId.firebaseio.com';
    if (parsedDatabaseUrl?.scheme != 'https' ||
        !(databaseUrl.contains('.firebasedatabase.app') ||
            databaseUrl.contains('.firebaseio.com')) ||
        !databaseMatchesProject) {
      return 'Staging Realtime Database URL must use its HTTPS Firebase '
          'endpoint for the configured project.';
    }
    if (databaseUrl.replaceFirst(RegExp(r'/+$'), '') !=
        confirmedStagingDatabaseUrl) {
      return 'Staging must use the confirmed Realtime Database URL.';
    }
    if (administratorUid.isEmpty ||
        administratorUid == _productionAdministratorUid) {
      return 'A dedicated staging administrator UID is required.';
    }
    if (deviceId.isEmpty) {
      return 'A staging device ID is required.';
    }
    if (deviceId != confirmedStagingDeviceId) {
      return 'Staging must use the confirmed esp32-frontdoor-1 device ID.';
    }
    if (gatewayMode != 'esp32_only') {
      return 'Staging remote hardware tests require GATEWAY_MODE=esp32_only.';
    }
    return null;
  }

  FirebaseOptions get firebaseOptions {
    if (!isStaging) return DefaultFirebaseOptions.currentPlatform;
    return FirebaseOptions(
      apiKey: apiKey,
      appId: appId,
      messagingSenderId: messagingSenderId,
      projectId: projectId,
      databaseURL: databaseUrl,
    );
  }
}
