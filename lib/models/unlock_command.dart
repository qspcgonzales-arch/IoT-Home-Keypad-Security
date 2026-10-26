enum UnlockCommandStatus { pending, accepted, unlocked, failed, expired }

class UnlockCommand {
  const UnlockCommand({
    required this.id,
    required this.type,
    required this.status,
    required this.requestedBy,
    required this.createdAtMillis,
    required this.expiresAtMillis,
    this.targetDeviceId,
    this.message,
  });

  final String id;
  final String type;
  final UnlockCommandStatus status;
  final String requestedBy;
  final int createdAtMillis;
  final int expiresAtMillis;
  final String? targetDeviceId;
  final String? message;

  Map<String, Object?> toDatabaseRecord({required Object serverTimestamp}) {
    final record = <String, Object?>{
      'type': type,
      'status': switch (status) {
        UnlockCommandStatus.pending => 'pending',
        UnlockCommandStatus.accepted => 'accepted',
        UnlockCommandStatus.unlocked => 'completed',
        UnlockCommandStatus.failed => 'failed',
        UnlockCommandStatus.expired => 'expired',
      },
      'requestedBy': requestedBy,
      'createdAt': serverTimestamp,
      'expiresAt': expiresAtMillis,
    };
    if (targetDeviceId != null) record['targetDeviceId'] = targetDeviceId;
    return record;
  }

  static UnlockCommandStatus? parseStatus(Object? value) => switch (value) {
    'pending' => UnlockCommandStatus.pending,
    'accepted' => UnlockCommandStatus.accepted,
    'completed' => UnlockCommandStatus.unlocked,
    'failed' => UnlockCommandStatus.failed,
    'expired' => UnlockCommandStatus.expired,
    _ => null,
  };

  static String labelFor(UnlockCommandStatus? status) => switch (status) {
    UnlockCommandStatus.pending => 'COMMAND QUEUED',
    UnlockCommandStatus.accepted => 'COMMAND ACCEPTED',
    UnlockCommandStatus.unlocked => 'COMMAND EXECUTED',
    UnlockCommandStatus.failed => 'FAILED',
    UnlockCommandStatus.expired => 'EXPIRED',
    null => 'REMOTE UNLOCK',
  };
}
