import 'package:flutter_test/flutter_test.dart';
import 'package:smart_home_security/models/unlock_command.dart';

void main() {
  const legacyCommand = UnlockCommand(
    id: 'legacy',
    type: 'unlock',
    status: UnlockCommandStatus.pending,
    requestedBy: 'admin',
    createdAtMillis: 1000,
    expiresAtMillis: 31000,
  );

  test('legacy command record does not acquire an unexpected target field', () {
    final record = legacyCommand.toDatabaseRecord(serverTimestamp: 1000);
    expect(record.containsKey('targetDeviceId'), isFalse);
  });

  test('targeted staging command includes its configured device ID', () {
    const command = UnlockCommand(
      id: 'stage',
      type: 'unlock',
      status: UnlockCommandStatus.pending,
      requestedBy: 'staging-admin',
      createdAtMillis: 1000,
      expiresAtMillis: 31000,
      targetDeviceId: 'esp32-frontdoor-1',
    );
    expect(
      command.toDatabaseRecord(serverTimestamp: 1000)['targetDeviceId'],
      'esp32-frontdoor-1',
    );
  });
}
