import 'package:flutter/material.dart';

import '../models/unlock_command.dart';
import '../services/door_service.dart';
import '../theme/app_theme.dart';
import '../widgets/event_tile.dart';
import '../widgets/status_badge.dart';

class DashboardScreen extends StatelessWidget {
  const DashboardScreen({super.key, required this.service});
  final DoorService service;

  Future<void> _confirmUnlock(BuildContext context) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        icon: const Icon(Icons.lock_open, color: AppTheme.navy),
        title: const Text('Unlock the door?'),
        content: Text(
          service.isSimulated
              ? 'The local simulator will acknowledge the request, unlock for '
                    '${service.unlockDurationSeconds} seconds, then relock automatically.'
              : 'The authorized device will process this secure command.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context, false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(context, true),
            child: const Text('Unlock'),
          ),
        ],
      ),
    );
    if (confirmed == true) await service.requestUnlock();
  }

  @override
  Widget build(BuildContext context) => AnimatedBuilder(
    animation: service,
    builder: (context, _) {
      final doorLabel = service.isLoading
          ? 'LOADING'
          : service.hasDoorStatus
          ? service.isLocked
                ? 'LOCKED'
                : 'UNLOCKED'
          : 'UNKNOWN';
      final doorColor = service.isLoading || !service.hasDoorStatus
          ? AppTheme.muted
          : service.isLocked
          ? const Color(0xFF16794B)
          : const Color(0xFFB54708);
      final gatewayLabel = service.isLoading
          ? 'LOADING'
          : !service.isDatabaseConnected
          ? 'OFFLINE'
          : !service.hasGatewayStatus
          ? 'UNKNOWN'
          : service.isDeviceReady
          ? 'ONLINE'
          : service.isHardware && service.isOnline && !service.isArduinoReady
          ? 'ARDUINO OFFLINE'
          : 'OFFLINE';
      final gatewayColor =
          service.isLoading ||
              (service.isDatabaseConnected && !service.hasGatewayStatus)
          ? AppTheme.muted
          : service.isDatabaseConnected && service.isOnline
          ? const Color(0xFF16794B)
          : const Color(0xFFB42318);
      final commandStatus = service.remoteUnlockStatus;
      final unlockButtonLabel = switch (commandStatus) {
        UnlockCommandStatus.pending ||
        UnlockCommandStatus.accepted => 'COMMAND PENDING',
        UnlockCommandStatus.unlocked => 'UNLOCKED',
        UnlockCommandStatus.failed ||
        UnlockCommandStatus.expired => 'Retry remote unlock',
        null =>
          !service.isLocked && service.hasDoorStatus
              ? 'Door unlocked'
              : service.canRequestUnlock
              ? 'Remote unlock'
              : 'Device unavailable',
      };
      return ListView(
        padding: const EdgeInsets.fromLTRB(20, 8, 20, 24),
        children: [
          Text(
            'Good to see you',
            style: Theme.of(context).textTheme.titleLarge?.copyWith(
              fontWeight: FontWeight.w700,
              color: AppTheme.navy,
            ),
          ),
          const SizedBox(height: 4),
          const Text(
            'Here is your home security at a glance.',
            style: TextStyle(color: AppTheme.muted),
          ),
          const SizedBox(height: 20),
          if (service.isLoading)
            const Padding(
              padding: EdgeInsets.only(bottom: 12),
              child: LinearProgressIndicator(),
            ),
          if (!service.isLoading && !service.isDatabaseConnected)
            Card(
              child: ListTile(
                leading: const Icon(Icons.cloud_off, color: Color(0xFFB42318)),
                title: const Text('Database offline'),
                subtitle: const Text(
                  'Live values may be unavailable or out of date.',
                ),
              ),
            ),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(20),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      const Expanded(
                        child: Text(
                          'Front door',
                          style: TextStyle(
                            fontSize: 16,
                            fontWeight: FontWeight.w700,
                            color: AppTheme.navy,
                          ),
                        ),
                      ),
                      StatusBadge(
                        label: doorLabel,
                        color: doorColor,
                        icon: !service.hasDoorStatus
                            ? Icons.help_outline
                            : service.isLocked
                            ? Icons.lock_outline
                            : Icons.lock_open,
                      ),
                    ],
                  ),
                  const SizedBox(height: 22),
                  Center(
                    child: Container(
                      width: 132,
                      height: 132,
                      decoration: BoxDecoration(
                        color: service.isLocked
                            ? const Color(0xFFEAF5EF)
                            : const Color(0xFFFFF4E5),
                        shape: BoxShape.circle,
                      ),
                      child: Icon(
                        service.isLocked
                            ? Icons.door_front_door_outlined
                            : Icons.meeting_room_outlined,
                        size: 64,
                        color: service.isLocked
                            ? const Color(0xFF16794B)
                            : const Color(0xFFB54708),
                      ),
                    ),
                  ),
                  const SizedBox(height: 20),
                  SizedBox(
                    width: double.infinity,
                    child: FilledButton.icon(
                      onPressed:
                          service.canRequestUnlock &&
                              service.isLocked &&
                              !service.isPending &&
                              service.isOnline
                          ? () => _confirmUnlock(context)
                          : null,
                      style: FilledButton.styleFrom(
                        backgroundColor: AppTheme.navy,
                        padding: const EdgeInsets.symmetric(vertical: 15),
                      ),
                      icon: service.isPending
                          ? const SizedBox(
                              width: 18,
                              height: 18,
                              child: CircularProgressIndicator(
                                strokeWidth: 2,
                                color: Colors.white,
                              ),
                            )
                          : const Icon(Icons.lock_open),
                      label: Text(
                        service.isPending
                            ? UnlockCommand.labelFor(commandStatus)
                            : unlockButtonLabel,
                      ),
                    ),
                  ),
                  if (commandStatus != null) ...[
                    const SizedBox(height: 10),
                    Text(
                      'Command status: ${UnlockCommand.labelFor(commandStatus)}',
                      textAlign: TextAlign.center,
                      style: TextStyle(
                        color:
                            commandStatus == UnlockCommandStatus.failed ||
                                commandStatus == UnlockCommandStatus.expired
                            ? const Color(0xFFB42318)
                            : AppTheme.navy,
                        fontSize: 12,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ],
                  if (service.feedback != null) ...[
                    const SizedBox(height: 12),
                    Text(
                      service.feedback!,
                      textAlign: TextAlign.center,
                      style: TextStyle(
                        color: service.feedback!.contains('failed')
                            ? const Color(0xFFB42318)
                            : AppTheme.muted,
                        fontSize: 13,
                      ),
                    ),
                  ],
                ],
              ),
            ),
          ),
          Card(
            child: Padding(
              padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 16),
              child: Row(
                children: [
                  const Icon(Icons.router_outlined, color: AppTheme.navy),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          service.canSimulateConnectivity
                              ? 'LOCAL DEMO — not connected to Firebase'
                              : service.isHardware
                              ? 'ESP32 gateway (hardware mode)'
                              : service.isSimulated
                              ? 'Firebase virtual device'
                              : 'ESP32 gateway',
                          style: TextStyle(
                            fontWeight: FontWeight.w600,
                            color: AppTheme.navy,
                          ),
                        ),
                        Text(
                          service.isSimulated
                              ? 'SIMULATED • command acknowledgements enabled'
                              : service.isHardware
                              ? service.isArduinoReady
                                    ? 'PHYSICAL • Arduino UART verified'
                                    : 'PHYSICAL • Arduino UART not verified'
                              : 'Device connectivity',
                          style: TextStyle(color: AppTheme.muted, fontSize: 12),
                        ),
                      ],
                    ),
                  ),
                  StatusBadge(
                    label: gatewayLabel,
                    color: gatewayColor,
                    icon: gatewayLabel == 'ONLINE'
                        ? Icons.wifi
                        : Icons.wifi_off,
                  ),
                ],
              ),
            ),
          ),
          const SizedBox(height: 18),
          Text(
            'Latest activity',
            style: Theme.of(context).textTheme.titleMedium?.copyWith(
              fontWeight: FontWeight.w700,
              color: AppTheme.navy,
            ),
          ),
          if (service.latestEvent case final event?)
            Card(child: EventTile(event: event))
          else
            const Card(
              child: Padding(
                padding: EdgeInsets.all(18),
                child: Text('No activity yet'),
              ),
            ),
          const SizedBox(height: 16),
          if (service.canSimulateConnectivity)
            TextButton.icon(
              onPressed: service.isPending
                  ? null
                  : () => service.setOnline(!service.isOnline),
              icon: Icon(service.isOnline ? Icons.wifi_off : Icons.wifi),
              label: Text(
                service.isOnline
                    ? 'Simulate gateway offline'
                    : 'Reconnect gateway',
              ),
            ),
        ],
      );
    },
  );
}
