import 'package:flutter/material.dart';

import '../services/door_service.dart';
import '../theme/app_theme.dart';
import '../widgets/event_tile.dart';

class AuditLogsScreen extends StatelessWidget {
  const AuditLogsScreen({super.key, required this.service});
  final DoorService service;

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: service,
    builder: (context, _) => ListView(
      padding: const EdgeInsets.fromLTRB(20, 8, 20, 24),
      children: [
        Text(
          'Security activity',
          style: Theme.of(context).textTheme.titleLarge?.copyWith(
            fontWeight: FontWeight.w700,
            color: AppTheme.navy,
          ),
        ),
        const SizedBox(height: 5),
        const Text(
          'Newest events appear first.',
          style: TextStyle(color: AppTheme.muted),
        ),
        const SizedBox(height: 18),
        if (service.isLoading)
          const Card(
            child: Padding(
              padding: EdgeInsets.all(20),
              child: Row(
                children: [
                  SizedBox(
                    width: 20,
                    height: 20,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  ),
                  SizedBox(width: 12),
                  Text('Loading audit history…'),
                ],
              ),
            ),
          )
        else if (!service.isDatabaseConnected || service.feedback != null)
          Card(
            child: ListTile(
              leading: const Icon(Icons.cloud_off, color: Color(0xFFB42318)),
              title: Text(
                service.feedback ?? 'Database offline',
                style: const TextStyle(color: Color(0xFFB42318)),
              ),
              subtitle: const Text('Audit history may be unavailable.'),
            ),
          ),
        if (!service.isLoading &&
            service.isDatabaseConnected &&
            service.feedback == null &&
            service.events.isEmpty)
          const Card(
            child: Padding(
              padding: EdgeInsets.all(24),
              child: Center(child: Text('No audit events yet.')),
            ),
          )
        else if (!service.isLoading && service.events.isNotEmpty)
          ...service.events.map(
            (event) => Padding(
              padding: const EdgeInsets.only(bottom: 10),
              child: Card(child: EventTile(event: event)),
            ),
          ),
      ],
    ),
  );
}
