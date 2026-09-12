import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../services/wifi_provision_coordinator.dart';
import '../../state/hcm_backend.dart';
import '../../theme/nisense_colors.dart';
import '../device/device_info_page.dart';
import '../widgets/common_widgets.dart';
import '../wifi/wifi_ap_picker_sheet.dart';

class SettingsPage extends StatefulWidget {
  const SettingsPage({super.key});

  @override
  State<SettingsPage> createState() => _SettingsPageState();
}

class _SettingsPageState extends State<SettingsPage> {
  final _ssidCtrl = TextEditingController();
  final _passCtrl = TextEditingController();
  final _patientCtrl = TextEditingController();
  bool _patientInitialized = false;
  late int _timeoutS;
  bool _sliderDragging = false;

  int _ppgCount = 500;
  int _glucSamples = 80;
  int _glucDelayMs = 2000;
  int _ppgDecimate = 1;
  int _schedSec = 600;
  int _tempIdleSec = 60;
  bool _samplingDragging = false;
  bool _samplingLoaded = false;

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    final b = context.read<HcmBackend>();
    // Populate patient name only once — not on every backend notify
    if (!_patientInitialized) {
      _patientCtrl.text = b.patientName;
      _patientInitialized = true;
    }
    // Only sync timeout from device when slider is not being dragged
    if (!_sliderDragging) {
      _timeoutS = b.screenTimeoutS > 0 ? b.screenTimeoutS : 60;
    }
    if (!_samplingDragging) {
      _ppgCount = b.samplingConfig.ppgSampleCount;
      _glucSamples = b.samplingConfig.glucoseNumSamples;
      _glucDelayMs = b.samplingConfig.glucoseDelayMs;
      _ppgDecimate = b.ppgDecimate;
      _schedSec = b.samplingConfig.isAdaptive
          ? b.samplingConfig.currentIntervalSec.clamp(60, 3600)
          : b.samplingConfig.scheduleIntervalSec.clamp(60, 3600);
      _tempIdleSec = b.samplingConfig.tempIdleIntervalSec.clamp(10, 3600);
    }
  }

  @override
  void dispose() {
    _ssidCtrl.dispose();
    _passCtrl.dispose();
    _patientCtrl.dispose();
    super.dispose();
  }

  Future<void> _ensureSamplingLoaded(HcmBackend b) async {
    if (_samplingLoaded || !b.connected || !b.isPaired) return;
    _samplingLoaded = true;
    await b.refreshSamplingConfig();
    if (!mounted) return;
    setState(() {
      _ppgCount = b.samplingConfig.ppgSampleCount;
      _glucSamples = b.samplingConfig.glucoseNumSamples;
      _glucDelayMs = b.samplingConfig.glucoseDelayMs;
      _ppgDecimate = b.ppgDecimate;
      _schedSec = b.samplingConfig.isAdaptive
          ? b.samplingConfig.currentIntervalSec.clamp(60, 3600)
          : b.samplingConfig.scheduleIntervalSec.clamp(60, 3600);
      _tempIdleSec = b.samplingConfig.tempIdleIntervalSec.clamp(10, 3600);
    });
  }

  Future<void> _scanAndProvision(HcmBackend b) async {
    final pick = await showWifiApPickerSheet(
      context,
      title: 'Choose Wi-Fi for device',
    );
    if (!mounted || pick == null) return;

    setState(() {
      _ssidCtrl.text = pick.ssid;
      _passCtrl.text = pick.password;
    });

    final messenger = ScaffoldMessenger.of(context);
    messenger.showSnackBar(
      SnackBar(content: Text('Provisioning ${pick.ssid} over BLE…')),
    );

    final coord = WifiProvisionCoordinator(backend: b);
    final result = await coord.provisionSelectedNetwork(
      ssid: pick.ssid,
      password: pick.password,
      instructions: (msg) {
        messenger.hideCurrentSnackBar();
        messenger.showSnackBar(SnackBar(content: Text(msg)));
      },
    );

    if (!mounted) return;
    messenger.hideCurrentSnackBar();
    messenger.showSnackBar(
      SnackBar(
        content: Text(result.message),
        backgroundColor: result.success ? null : NiSenseColors.accentRed,
      ),
    );
  }

  static String _formatScheduleInterval(int sec) {
    final min = sec / 60.0;
    if (sec % 60 == 0) {
      return '${min.toStringAsFixed(0)} min';
    }
    return '${min.toStringAsFixed(1)} min (${sec}s)';
  }

  Widget _samplingSlider({
    required String label,
    required String valueText,
    required double value,
    required double min,
    required double max,
    required int divisions,
    required bool enabled,
    required ValueChanged<double> onChanged,
    required ValueChanged<double> onChangeEnd,
  }) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Row(
          children: [
            Expanded(child: Text(label)),
            Text(
              valueText,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: NiSenseColors.textMuted,
                  ),
            ),
          ],
        ),
        Slider(
          value: value.clamp(min, max),
          min: min,
          max: max,
          divisions: divisions,
          label: valueText,
          onChanged: enabled
              ? (v) {
                  _samplingDragging = true;
                  onChanged(v);
                }
              : null,
          onChangeEnd: enabled
              ? (v) {
                  _samplingDragging = false;
                  onChangeEnd(v);
                }
              : null,
        ),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    final b = context.watch<HcmBackend>();
    final canSample = b.connected && b.isPaired;
    if (canSample && !_samplingLoaded) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) _ensureSamplingLoaded(b);
      });
    }
    final ppgSecs = (_ppgCount / 25.0);

    return ListView(
      padding: const EdgeInsets.all(12),
      children: [
        // Patient name — always at top
        GlassCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Patient', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 8),
              TextField(
                controller: _patientCtrl,
                decoration: const InputDecoration(
                  labelText: 'Patient name',
                  border: OutlineInputBorder(),
                ),
                textInputAction: TextInputAction.done,
                onEditingComplete: () => b.setPatientName(_patientCtrl.text.trim()),
                onSubmitted: (v) => b.setPatientName(v.trim()),
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),

        // Sleep timeout
        GlassCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Display', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 4),
              Row(
                children: [
                  Expanded(
                    child: Slider(
                      value: _timeoutS.toDouble().clamp(10, 600),
                      min: 10,
                      max: 600,
                      divisions: 59,
                      label: '${_timeoutS}s',
                      onChanged: b.connected
                          ? (v) => setState(() {
                                _sliderDragging = true;
                                _timeoutS = v.round();
                              })
                          : null,
                      onChangeEnd: b.connected
                          ? (v) {
                              _sliderDragging = false;
                              b.setScreenTimeout(v.round());
                            }
                          : null,
                    ),
                  ),
                  SizedBox(
                    width: 80,
                    child: Text(
                      'Sleep ${_timeoutS}s',
                      style: Theme.of(context).textTheme.bodySmall,
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),

        // Sampling test knobs (BLE f01b + f012)
        GlassCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Sampling (test)', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 4),
              Text(
                'Tweaks PPG acquisition length and glucose ADC cadence over BLE. '
                'Requires pair. Applies to the next measurement started from the app.',
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: NiSenseColors.textMuted,
                    ),
              ),
              if (!b.isPaired && b.connected)
                Padding(
                  padding: const EdgeInsets.only(top: 8),
                  child: Text(
                    'Pair the device to edit sampling.',
                    style: Theme.of(context).textTheme.bodySmall?.copyWith(
                          color: NiSenseColors.accentRed,
                        ),
                  ),
                ),
              const SizedBox(height: 8),
              _samplingSlider(
                label: 'PPG sample count',
                valueText: '$_ppgCount (~${ppgSecs.toStringAsFixed(1)}s @ 25 Hz)',
                value: _ppgCount.toDouble(),
                min: 50,
                max: 1000,
                divisions: 95,
                enabled: canSample,
                onChanged: (v) => setState(() => _ppgCount = v.round()),
                onChangeEnd: (v) => b.setSamplingConfig(ppgSampleCount: v.round()),
              ),
              _samplingSlider(
                label: 'Glucose ADC samples',
                valueText: '$_glucSamples',
                value: _glucSamples.toDouble(),
                min: 10,
                max: 500,
                divisions: 49,
                enabled: canSample,
                onChanged: (v) => setState(() => _glucSamples = v.round()),
                onChangeEnd: (v) => b.setSamplingConfig(glucoseNumSamples: v.round()),
              ),
              _samplingSlider(
                label: 'Glucose sample interval',
                valueText: '$_glucDelayMs ms',
                value: _glucDelayMs.toDouble(),
                min: 100,
                max: 5000,
                divisions: 49,
                enabled: canSample,
                onChanged: (v) => setState(() => _glucDelayMs = v.round()),
                onChangeEnd: (v) => b.setSamplingConfig(glucoseDelayMs: v.round()),
              ),
              _samplingSlider(
                label: 'PPG BLE stream decimate',
                valueText: '1/$_ppgDecimate',
                value: _ppgDecimate.toDouble(),
                min: 1,
                max: 33,
                divisions: 32,
                enabled: canSample,
                onChanged: (v) => setState(() => _ppgDecimate = v.round()),
                onChangeEnd: (v) => b.setPpgDecimate(v.round()),
              ),
              SwitchListTile(
                contentPadding: EdgeInsets.zero,
                title: const Text('Disable proximity'),
                subtitle: const Text(
                  'Bench / sched / record test — ignore wear. '
                  'WORN_GOOD needs ≥30k (skin); table top should stay blocked.',
                ),
                value: b.samplingConfig.disableProximity,
                onChanged: canSample
                    ? (v) => b.setSamplingConfig(disableProximity: v)
                    : null,
              ),
              SwitchListTile(
                contentPadding: EdgeInsets.zero,
                title: const Text('Auto measurement schedule'),
                subtitle: const Text(
                  'Wear-gated health cycle. Adaptive ladder: '
                  '5 / 10 / 15 / 20 / 30 min (starts at 10; glucose/battery adjust).',
                ),
                value: b.samplingConfig.autoEnabled,
                onChanged: canSample
                    ? (v) => b.setSamplingConfig(autoEnabled: v)
                    : null,
              ),
              _samplingSlider(
                label: 'Schedule interval',
                valueText: b.samplingConfig.isAdaptive
                    ? 'Adaptive · now ${_formatScheduleInterval(b.samplingConfig.currentIntervalSec)}'
                    : 'Fixed · ${_formatScheduleInterval(_schedSec)}',
                value: _schedSec.toDouble().clamp(60, 3600),
                min: 60,
                max: 3600,
                divisions: 59,
                enabled: canSample,
                onChanged: (v) => setState(() => _schedSec = v.round()),
                onChangeEnd: (v) =>
                    b.setSamplingConfig(scheduleIntervalSec: v.round()),
              ),
              Text(
                b.samplingConfig.isAdaptive
                    ? 'Current ladder step: '
                        '${_formatScheduleInterval(b.samplingConfig.currentIntervalSec)} '
                        '(${b.samplingConfig.currentIntervalSec}s). '
                        'Drag slider to lock a fixed interval (1–60 min).'
                    : 'Fixed every ${_formatScheduleInterval(_schedSec)}. '
                        'Tap below to return to adaptive ladder.',
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: NiSenseColors.textMuted,
                    ),
              ),
              Align(
                alignment: Alignment.centerLeft,
                child: TextButton(
                  onPressed: canSample
                      ? () => b.setSamplingConfig(scheduleIntervalSec: 0)
                      : null,
                  child: const Text('Use adaptive ladder (5–30 min)'),
                ),
              ),
              _samplingSlider(
                label: 'Idle temp interval',
                valueText: _formatScheduleInterval(_tempIdleSec),
                value: _tempIdleSec.toDouble().clamp(10, 3600),
                min: 10,
                max: 3600,
                divisions: 359,
                enabled: canSample,
                onChanged: (v) => setState(() => _tempIdleSec = v.round()),
                onChangeEnd: (v) =>
                    b.setSamplingConfig(tempIdleIntervalSec: v.round()),
              ),
              Text(
                'How often wrist/SoC temperature is logged between Measure cycles '
                '(paused during a cycle). Measure cycles already include TEMP_PRE/POST.',
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                      color: NiSenseColors.textMuted,
                    ),
              ),
              Align(
                alignment: Alignment.centerRight,
                child: TextButton.icon(
                  onPressed: canSample
                      ? () async {
                          _samplingLoaded = false;
                          await _ensureSamplingLoaded(b);
                        }
                      : null,
                  icon: const Icon(Icons.refresh, size: 18),
                  label: const Text('Reload from device'),
                ),
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),

        // Wi-Fi
        GlassCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('WiFi', style: Theme.of(context).textTheme.titleMedium),
              const SizedBox(height: 4),
              Text(
                'Scan nearby APs on this phone, pick one, enter the password, '
                'then write credentials to the device over BLE.',
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: NiSenseColors.textMuted,
                ),
              ),
              const SizedBox(height: 8),
              TextField(
                controller: _ssidCtrl,
                decoration: const InputDecoration(
                  labelText: 'SSID',
                  border: OutlineInputBorder(),
                ),
              ),
              const SizedBox(height: 8),
              TextField(
                controller: _passCtrl,
                decoration: const InputDecoration(
                  labelText: 'Password',
                  border: OutlineInputBorder(),
                ),
                obscureText: true,
              ),
              const SizedBox(height: 6),
              Text(
                'Status: ${b.wifiConnected > 0 ? 'Connected' : 'Disconnected'} · ${b.wifiIp}'
                '${b.wifiSsid.isNotEmpty ? ' · ${b.wifiSsid}' : ''}',
                style: Theme.of(context).textTheme.bodySmall,
              ),
              const SizedBox(height: 8),
              Wrap(
                spacing: 8,
                runSpacing: 8,
                children: [
                  FilledButton.icon(
                    onPressed: b.connected && b.isPaired
                        ? () => _scanAndProvision(b)
                        : null,
                    icon: const Icon(Icons.wifi_find, size: 18),
                    label: const Text('Scan & provision'),
                  ),
                  FilledButton(
                    onPressed: b.connected && b.isPaired
                        ? () => b.setWifiCredentials(
                              _ssidCtrl.text,
                              _passCtrl.text,
                            )
                        : null,
                    child: const Text('Save'),
                  ),
                  OutlinedButton(
                    onPressed: b.connected ? () => b.wifiConnect() : null,
                    child: const Text('Connect'),
                  ),
                  OutlinedButton(
                    onPressed: b.connected ? () => b.wifiDisconnect() : null,
                    child: const Text('Disconnect'),
                  ),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),

        // Device Info link
        ListTile(
          shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
          tileColor: Theme.of(context).colorScheme.surfaceContainerHighest,
          leading: const Icon(Icons.info_outline),
          title: const Text('Device Info'),
          subtitle: const Text('ID, firmware, record status'),
          trailing: const Icon(Icons.chevron_right),
          onTap: () => Navigator.of(context).push(
            MaterialPageRoute<void>(builder: (_) => const DeviceInfoPage()),
          ),
        ),
      ],
    );
  }
}
