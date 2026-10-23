import 'package:flutter/material.dart';

import '../models/security_event.dart';
import '../theme/app_theme.dart';

class EventTile extends StatelessWidget {
  const EventTile({super.key, required this.event});
  final SecurityEvent event;

  IconData get _icon => switch (event.type) {
    SecurityEventType.accessGranted => Icons.verified_user_outlined,
    SecurityEventType.accessDenied => Icons.gpp_bad_outlined,
    SecurityEventType.remoteUnlock => Icons.lock_open_outlined,
    SecurityEventType.automaticRelock => Icons.lock_outline,
    SecurityEventType.securityLockout => Icons.warning_amber_rounded,
    SecurityEventType.commandFailed => Icons.error_outline,
    SecurityEventType.commandExpired => Icons.timer_off_outlined,
  };

  Color get _color => switch (event.type) {
    SecurityEventType.accessGranted ||
    SecurityEventType.automaticRelock => const Color(0xFF16794B),
    SecurityEventType.accessDenied ||
    SecurityEventType.securityLockout => const Color(0xFFB42318),
    SecurityEventType.commandFailed ||
    SecurityEventType.commandExpired => const Color(0xFFB42318),
    SecurityEventType.remoteUnlock => AppTheme.navy,
  };

  @override
  Widget build(BuildContext context) {
    final time = MaterialLocalizations.of(
      context,
    ).formatTimeOfDay(TimeOfDay.fromDateTime(event.timestamp));
    final date = MaterialLocalizations.of(
      context,
    ).formatShortDate(event.timestamp);
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 14),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            width: 40,
            height: 40,
            decoration: BoxDecoration(
              color: _color.withValues(alpha: 0.10),
              borderRadius: BorderRadius.circular(13),
            ),
            child: Icon(_icon, color: _color, size: 21),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  event.title,
                  style: const TextStyle(
                    fontWeight: FontWeight.w700,
                    color: AppTheme.navy,
                  ),
                ),
                const SizedBox(height: 3),
                Text(
                  event.description,
                  style: const TextStyle(color: AppTheme.muted, fontSize: 12),
                ),
                const SizedBox(height: 6),
                Text(
                  '$date • $time',
                  style: const TextStyle(color: AppTheme.muted, fontSize: 11),
                ),
              ],
            ),
          ),
          const SizedBox(width: 8),
          Icon(Icons.circle, color: _color, size: 8),
        ],
      ),
    );
  }
}
