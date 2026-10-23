enum SecurityEventType {
  accessGranted,
  accessDenied,
  remoteUnlock,
  automaticRelock,
  securityLockout,
  commandFailed,
  commandExpired,
}

class SecurityEvent {
  const SecurityEvent({
    required this.type,
    required this.timestamp,
    required this.description,
  });

  final SecurityEventType type;
  final DateTime timestamp;
  final String description;

  String get title => switch (type) {
    SecurityEventType.accessGranted => 'Access granted',
    SecurityEventType.accessDenied => 'Access denied',
    SecurityEventType.remoteUnlock => 'Remote unlock',
    SecurityEventType.automaticRelock => 'Automatic relock',
    SecurityEventType.securityLockout => 'Security lockout',
    SecurityEventType.commandFailed => 'Command failed',
    SecurityEventType.commandExpired => 'Command expired',
  };
}
