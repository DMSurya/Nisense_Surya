import 'dart:async';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../ble/hcm_ble_client.dart';
import '../state/hcm_backend.dart';
import '../theme/nisense_colors.dart';
import '../theme/nisense_icons.dart';
import 'dashboard/dashboard_page.dart';
import 'firmware/firmware_page.dart';
import 'raw/raw_page.dart';
import 'scan/scan_page.dart';
import 'server/server_page.dart';
import 'settings/settings_page.dart';
import 'sync/sync_page.dart';

class AppShell extends StatefulWidget {
  const AppShell({super.key});

  @override
  State<AppShell> createState() => _AppShellState();
}

class _AppShellState extends State<AppShell> {
  int _index = 0;

  // Bluetooth/Scan restored as first tab — it was dropped from the nav during
  // the Settings/Charts/Logs redesign, leaving no way to scan/connect.
  static const _pages = [
    ScanPage(),
    DashboardPage(),
    RawPage(),
    SettingsPage(),
    SyncPage(),
    FirmwarePage(),
  ];

  static BleNotifyProfile _profileForTab(int index) {
    switch (index) {
      case 1:
        return BleNotifyProfile.dashboard;
      case 2:
        return BleNotifyProfile.raw;
      default:
        // Bluetooth, Settings, Sync, Firmware — keep CCC light until an
        // exclusive op (sync/DFU) takes over.
        return BleNotifyProfile.minimal;
    }
  }

  void _selectTab(int i) {
    final b = context.read<HcmBackend>();
    if (b.navigationLocked) {
      final reason = b.navigationLockReason ?? 'Operation in progress';
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Stay on this screen — $reason')),
      );
      return;
    }
    setState(() => _index = i);
    unawaited(b.setShellNotifyProfile(_profileForTab(i)));
  }

  void _showLockedSnack() {
    final reason =
        context.read<HcmBackend>().navigationLockReason ?? 'Operation in progress';
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text('Stay on this screen — $reason')),
    );
  }

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      unawaited(
        context.read<HcmBackend>().setShellNotifyProfile(_profileForTab(_index)),
      );
    });
  }

  @override
  Widget build(BuildContext context) {
    final b = context.watch<HcmBackend>();
    final locked = b.navigationLocked;
    return Scaffold(
      appBar: AppBar(
        title: Row(
          children: [
            NiSenseIcons.logoImage(size: 26),
            const SizedBox(width: 10),
            const Text('NiSense Link'),
          ],
        ),
        backgroundColor: NiSenseColors.bgHeader,
        actions: [
          IconButton(
            tooltip: 'Server / Cloud',
            icon: const Icon(Icons.cloud_outlined),
            onPressed: locked
                ? _showLockedSnack
                : () => Navigator.of(context).push(
                      MaterialPageRoute<void>(
                        builder: (_) => Scaffold(
                          appBar: AppBar(
                            title: const Text('Server'),
                            backgroundColor: NiSenseColors.bgHeader,
                          ),
                          body: const ServerPage(),
                        ),
                      ),
                    ),
          ),
        ],
      ),
      body: Column(
        children: [
          if (locked)
            Container(
              width: double.infinity,
              color: NiSenseColors.bgHeader,
              padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
              child: Row(
                children: [
                  const Icon(Icons.lock_outline, size: 18),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      b.navigationLockReason ?? 'Operation in progress',
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                ],
              ),
            ),
          Expanded(
            child: IndexedStack(
              index: _index,
              children: _pages,
            ),
          ),
        ],
      ),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _index,
        onDestinationSelected: _selectTab,
        destinations: [
          NavigationDestination(
            icon: NiSenseIcons.navIcon('scan'),
            selectedIcon: NiSenseIcons.navIcon('scan', active: true),
            label: 'Bluetooth',
          ),
          NavigationDestination(
            icon: NiSenseIcons.navIcon('dashboard'),
            selectedIcon: NiSenseIcons.navIcon('dashboard', active: true),
            label: 'Dashboard',
          ),
          NavigationDestination(
            icon: NiSenseIcons.navIcon('charts'),
            selectedIcon: NiSenseIcons.navIcon('charts', active: true),
            label: 'Raw',
          ),
          NavigationDestination(
            icon: NiSenseIcons.navIcon('settings'),
            selectedIcon: NiSenseIcons.navIcon('settings', active: true),
            label: 'Settings',
          ),
          NavigationDestination(
            icon: NiSenseIcons.navIcon('logs'),
            selectedIcon: NiSenseIcons.navIcon('logs', active: true),
            label: 'Sync',
          ),
          NavigationDestination(
            icon: NiSenseIcons.navIcon('firmware'),
            selectedIcon: NiSenseIcons.navIcon('firmware', active: true),
            label: 'Firmware',
          ),
        ],
      ),
    );
  }
}
