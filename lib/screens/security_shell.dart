import 'package:flutter/material.dart';

import '../services/door_service.dart';
import '../theme/app_theme.dart';
import 'audit_logs_screen.dart';
import 'dashboard_screen.dart';
import 'settings_screen.dart';

class SecurityShell extends StatefulWidget {
  const SecurityShell({
    super.key,
    required this.email,
    required this.doorService,
    required this.onLogout,
  });

  final String email;
  final DoorService doorService;
  final VoidCallback onLogout;

  @override
  State<SecurityShell> createState() => _SecurityShellState();
}

class _SecurityShellState extends State<SecurityShell> {
  int _selectedIndex = 0;

  @override
  Widget build(BuildContext context) {
    final pages = [
      DashboardScreen(service: widget.doorService),
      AuditLogsScreen(service: widget.doorService),
      SettingsScreen(service: widget.doorService),
    ];
    return Scaffold(
      appBar: AppBar(
        title: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(switch (_selectedIndex) {
              0 => 'Security terminal',
              1 => 'Audit logs',
              _ => 'Settings',
            }, style: const TextStyle(fontWeight: FontWeight.w700)),
            Text(
              widget.email,
              style: const TextStyle(fontSize: 12, color: AppTheme.muted),
            ),
          ],
        ),
        actions: [
          IconButton(
            tooltip: 'Sign out',
            onPressed: widget.onLogout,
            icon: const Icon(Icons.logout),
          ),
        ],
      ),
      body: IndexedStack(index: _selectedIndex, children: pages),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _selectedIndex,
        onDestinationSelected: (index) =>
            setState(() => _selectedIndex = index),
        destinations: const [
          NavigationDestination(
            icon: Icon(Icons.space_dashboard_outlined),
            selectedIcon: Icon(Icons.space_dashboard),
            label: 'Dashboard',
          ),
          NavigationDestination(
            icon: Icon(Icons.receipt_long_outlined),
            selectedIcon: Icon(Icons.receipt_long),
            label: 'Audit logs',
          ),
          NavigationDestination(
            icon: Icon(Icons.settings_outlined),
            selectedIcon: Icon(Icons.settings),
            label: 'Settings',
          ),
        ],
      ),
    );
  }
}
