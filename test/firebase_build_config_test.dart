import 'package:flutter_test/flutter_test.dart';
import 'package:smart_home_security/services/firebase_build_config.dart';

void main() {
  FirebaseBuildConfig stagingConfig({
    String projectId = 'iothomekeypadsecurity-staging',
    String administratorUid = 'staging-admin-uid',
    String gatewayMode = 'esp32_only',
    String appId = '1:637464771451:android:test-only-app-id',
    String deviceId = 'esp32-frontdoor-1',
  }) => FirebaseBuildConfig(
    environment: 'staging',
    flavor: 'staging',
    projectId: projectId,
    apiKey: 'staging-web-api-key',
    appId: appId,
    messagingSenderId: '637464771451',
    databaseUrl: FirebaseBuildConfig.confirmedStagingDatabaseUrl,
    administratorUid: administratorUid,
    deviceId: deviceId,
    gatewayMode: gatewayMode,
  );

  test('staging requires a separate explicit Firebase Android app', () {
    expect(stagingConfig().validationError, isNull);
    expect(
      stagingConfig(projectId: 'smart-home-security-tip').validationError,
      contains('confirmed iothomekeypadsecurity-staging'),
    );
    expect(
      stagingConfig(projectId: 'demo-smart-home').validationError,
      isNotNull,
    );
    expect(
      stagingConfig(appId: '1:987654321:android:wrong-sender')
          .validationError,
      contains('sender ID do not match'),
    );
    expect(
      stagingConfig(appId: '1:637464771451:android')
          .validationError,
      contains('Android app ID and sender ID do not match'),
    );
  });

  test('staging requires distinct administrator, device targeting and mode', () {
    expect(
      stagingConfig(administratorUid: 'ERxtj2dQuQRvv3Duw8Ud4DNMErD3')
          .validationError,
      contains('administrator UID'),
    );
    expect(
      stagingConfig(gatewayMode: 'simulated').validationError,
      contains('GATEWAY_MODE=esp32_only'),
    );
    expect(
      stagingConfig(deviceId: 'another-device-id').validationError,
      contains('confirmed esp32-frontdoor-1 device ID'),
    );
  });

  test('template placeholders cannot initialize Firebase', () {
    expect(
      stagingConfig(appId: 'REPLACE_WITH_STAGING_ANDROID_APP_ID')
          .validationError,
      contains('Replace the staging Firebase example values'),
    );
    expect(
      stagingConfig()
          .copyWithTestDatabaseUrl('http://127.0.0.1:9000')
          .validationError,
      contains('HTTPS Firebase endpoint for the configured project'),
    );
  });
}

extension on FirebaseBuildConfig {
  FirebaseBuildConfig copyWithTestDatabaseUrl(String value) =>
      FirebaseBuildConfig(
        environment: environment,
        flavor: flavor,
        projectId: projectId,
        apiKey: apiKey,
        appId: appId,
        messagingSenderId: messagingSenderId,
        databaseUrl: value,
        administratorUid: administratorUid,
        deviceId: deviceId,
        gatewayMode: gatewayMode,
      );
}
