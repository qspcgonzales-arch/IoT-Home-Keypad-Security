import 'package:flutter/material.dart';

import '../services/door_service.dart';
import '../theme/app_theme.dart';

class SettingsScreen extends StatefulWidget {
  const SettingsScreen({super.key, required this.service});

  final DoorService service;

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  late int _selectedSeconds;
  bool _dirty = false;
  bool _saving = false;

  @override
  void initState() {
    super.initState();
    _selectedSeconds = widget.service.unlockDurationSeconds;
    widget.service.addListener(_syncDuration);
  }

  void _syncDuration() {
    if (!mounted || _dirty || _saving) return;
    setState(() => _selectedSeconds = widget.service.unlockDurationSeconds);
  }

  @override
  void dispose() {
    widget.service.removeListener(_syncDuration);
    super.dispose();
  }

  Future<void> _save() async {
    setState(() => _saving = true);
    final saved = await widget.service.setUnlockDurationSeconds(
      _selectedSeconds,
    );
    if (!mounted) return;
    setState(() {
      _saving = false;
      _dirty = !saved;
    });
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text(
          saved
              ? 'Unlock and relock duration saved.'
              : widget.service.feedback ?? 'Could not save this setting.',
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) => ListView(
    padding: const EdgeInsets.fromLTRB(20, 20, 20, 32),
    children: [
      Text(
        'Door timing',
        style: Theme.of(context).textTheme.titleLarge?.copyWith(
          color: AppTheme.navy,
          fontWeight: FontWeight.w700,
        ),
      ),
      const SizedBox(height: 8),
      const Text(
        'Choose how long the door stays unlocked before it relocks automatically. '
        'This applies to remote unlocks and keypad access.',
        style: TextStyle(color: AppTheme.muted, height: 1.45),
      ),
      const SizedBox(height: 20),
      Card(
        child: Padding(
          padding: const EdgeInsets.all(20),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  const Icon(Icons.timer_outlined, color: AppTheme.navy),
                  const SizedBox(width: 12),
                  const Expanded(
                    child: Text(
                      'Unlock duration',
                      style: TextStyle(
                        color: AppTheme.navy,
                        fontWeight: FontWeight.w700,
                      ),
                    ),
                  ),
                  Text(
                    '$_selectedSeconds sec',
                    style: const TextStyle(
                      color: AppTheme.navy,
                      fontSize: 18,
                      fontWeight: FontWeight.w700,
                    ),
                  ),
                ],
              ),
              Slider(
                min: 4,
                max: 30,
                divisions: 26,
                value: _selectedSeconds.toDouble(),
                label: '$_selectedSeconds seconds',
                onChanged: _saving
                    ? null
                    : (value) => setState(() {
                        _selectedSeconds = value.round();
                        _dirty =
                            _selectedSeconds !=
                            widget.service.unlockDurationSeconds;
                      }),
              ),
              const Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Text('4 seconds', style: TextStyle(color: AppTheme.muted)),
                  Text('30 seconds', style: TextStyle(color: AppTheme.muted)),
                ],
              ),
              const SizedBox(height: 16),
              SizedBox(
                width: double.infinity,
                child: FilledButton.icon(
                  onPressed: _saving || !_dirty ? null : _save,
                  icon: _saving
                      ? const SizedBox(
                          width: 18,
                          height: 18,
                          child: CircularProgressIndicator(
                            strokeWidth: 2,
                            color: Colors.white,
                          ),
                        )
                      : const Icon(Icons.save_outlined),
                  label: Text(_saving ? 'Saving…' : 'Save setting'),
                  style: FilledButton.styleFrom(
                    backgroundColor: AppTheme.navy,
                    padding: const EdgeInsets.symmetric(vertical: 14),
                  ),
                ),
              ),
            ],
          ),
        ),
      ),
      Card(
        child: ListTile(
          leading: const Icon(Icons.shield_outlined, color: AppTheme.navy),
          title: const Text('Automatic relock'),
          subtitle: Text(
            'The door locks again after $_selectedSeconds seconds. '
            'Default duration is 4 seconds.',
          ),
        ),
      ),
      if (widget.service.canSimulateConnectivity)
        const Padding(
          padding: EdgeInsets.only(top: 8),
          child: Text(
            'LOCAL DEMO — this setting is stored only for this app session.',
            textAlign: TextAlign.center,
            style: TextStyle(color: AppTheme.muted, fontSize: 12),
          ),
        ),
    ],
  );
}
