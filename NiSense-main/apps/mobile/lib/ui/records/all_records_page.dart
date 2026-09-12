import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../protocol/hcm_protocol.dart';
import '../../services/record_local_store.dart';
import '../../services/record_xlsx_exporter.dart';
import '../../state/hcm_backend.dart';
import '../../theme/nisense_colors.dart';

/// Browses every record ever synced from any device, persisted locally in
/// SQLite (survives even after the device deletes its own copy post-ACK).
/// Full cumulative history is exportable in one go via "Full Export".
class AllRecordsPage extends StatefulWidget {
  const AllRecordsPage({super.key});

  @override
  State<AllRecordsPage> createState() => _AllRecordsPageState();
}

class _AllRecordsPageState extends State<AllRecordsPage> {
  bool _loading = true;
  bool _exporting = false;
  String? _error;
  int? _typeFilter; // null = All
  List<StoredRecord> _all = [];
  Map<int, int> _counts = {};

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    setState(() => _loading = true);
    final all = await RecordLocalStore.instance.getAll();
    final counts = await RecordLocalStore.instance.countsByType();
    if (!mounted) return;
    setState(() {
      _all = all;
      _counts = counts;
      _loading = false;
    });
  }

  String _typeName(int t) {
    switch (t) {
      case recTypeGlucose:
        return 'Glucose';
      case recTypeVitals:
        return 'Vitals';
      case recTypeTemp:
        return 'Temp';
      case recTypePpgRaw:
        return 'PPG Raw';
      case recTypeGlucoseRaw:
        return 'Glucose Raw';
      default:
        return 'Type $t';
    }
  }

  /// Historical records show exactly what was captured — a genuine 0 stays
  /// visible as "--" (clearly marked invalid/missing) rather than being
  /// silently replaced by an older value, which would misrepresent what the
  /// device actually logged at that time.
  String _intOrDash(int v) => v == 0 ? '--' : '$v';
  String _doubleOrDash(double v, int decimals) =>
      v == 0.0 ? '--' : v.toStringAsFixed(decimals);

  String _summarize(StoredRecord r) {
    switch (r.type) {
      case recTypeGlucose:
        final f = decodeGlucoseRecordFields(r.payload);
        if (f.isEmpty) return 'Glucose record';
        final glucose = _intOrDash(f['glucose_mg_dl'] as int);
        final homaIr = _doubleOrDash(f['homa_ir_index'] as double, 2);
        return '$glucose mg/dL  ·  HOMA-IR $homaIr';
      case recTypeVitals:
        final f = decodeVitalsRecordFields(r.payload);
        if (f.isEmpty) return 'Vitals record';
        final hr = _intOrDash(f['hr_bpm'] as int);
        final spo2 = _intOrDash(f['spo2_percent'] as int);
        final hb = _doubleOrDash(f['hb_g_dl'] as double, 1);
        return 'HR $hr bpm  ·  SpO₂ $spo2%  ·  Hb $hb g/dL';
      case recTypeTemp:
        final f = decodeTempRecordFields(r.payload);
        if (f.isEmpty) return 'Temp record';
        final skin = _doubleOrDash(f['skin_temp_c'] as double, 1);
        final soc = _doubleOrDash(f['soc_temp_c'] as double, 1);
        return 'Skin $skin°C  ·  SoC $soc°C';
      case recTypePpgRaw:
        final f = decodePpgRawChunkFields(r.payload);
        if (f.isEmpty) return 'PPG raw chunk';
        final h = f['header'] as Map;
        final samples = f['samples'] as List<Map<String, dynamic>>;
        final count = samples.length;
        if (count == 0) {
          return 'Chunk ${h['chunk_index']}/${h['chunk_count']}  ·  0 samples  ·  parent #${r.parentId}';
        }
        final last = samples.last;
        final ir = _intOrDash(last['ir'] as int);
        final red = _intOrDash(last['red'] as int);
        final green = _intOrDash(last['green'] as int);
        return 'Chunk ${h['chunk_index']}/${h['chunk_count']}  ·  $count samples  ·  IR $ir  Red $red  Green $green  ·  parent #${r.parentId}';
      case recTypeGlucoseRaw:
        final f = decodeGlucoseRawChunkFields(r.payload);
        if (f.isEmpty) return 'Glucose raw chunk';
        final h = f['header'] as Map;
        return 'Chunk ${h['chunk_index']}/${h['chunk_count']}  ·  ${(f['samples'] as List).length} samples  ·  parent #${r.parentId}';
      default:
        return '${r.payload.length} bytes';
    }
  }

  Future<void> _fullExport() async {
    if (_all.isEmpty) return;
    setState(() {
      _exporting = true;
      _error = null;
    });
    try {
      final patient = context.read<HcmBackend>().patientName;
      final path = await RecordXlsxExporter.exportAllFromStore(
        patientName: patient.isNotEmpty ? patient : 'Patient',
      );
      if (!mounted) return;
      if (path == null) {
        setState(() => _error = 'No records on this phone to export');
      }
    } catch (e) {
      setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _exporting = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    final filtered = _typeFilter == null
        ? _all
        : _all.where((r) => r.type == _typeFilter).toList();
    final total = _all.length;

    return Scaffold(
      appBar: AppBar(
        title: const Text('All Records'),
        backgroundColor: NiSenseColors.bgHeader,
        actions: [
          IconButton(
            tooltip: 'Refresh',
            icon: const Icon(Icons.refresh),
            onPressed: _loading ? null : _load,
          ),
        ],
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : Column(
              children: [
                Padding(
                  padding: const EdgeInsets.fromLTRB(12, 12, 12, 4),
                  child: Row(
                    children: [
                      Expanded(
                        child: Text(
                          '$total records stored on this phone',
                          style: Theme.of(context).textTheme.titleSmall,
                        ),
                      ),
                      FilledButton.icon(
                        icon: _exporting
                            ? const SizedBox(
                                width: 16,
                                height: 16,
                                child: CircularProgressIndicator(strokeWidth: 2),
                              )
                            : const Icon(Icons.ios_share, size: 18),
                        label: Text(_exporting ? 'Exporting…' : 'Full Export'),
                        onPressed: (_exporting || _all.isEmpty) ? null : _fullExport,
                      ),
                    ],
                  ),
                ),
                if (_error != null)
                  Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 12),
                    child: Text(_error!, style: TextStyle(color: NiSenseColors.accentRed)),
                  ),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
                  child: Wrap(
                    spacing: 6,
                    children: [
                      ChoiceChip(
                        label: Text('All ($total)'),
                        selected: _typeFilter == null,
                        onSelected: (_) => setState(() => _typeFilter = null),
                      ),
                      for (final t in [
                        recTypeGlucose,
                        recTypeVitals,
                        recTypeTemp,
                        recTypePpgRaw,
                        recTypeGlucoseRaw,
                      ])
                        if ((_counts[t] ?? 0) > 0)
                          ChoiceChip(
                            label: Text('${_typeName(t)} (${_counts[t]})'),
                            selected: _typeFilter == t,
                            onSelected: (_) => setState(() => _typeFilter = t),
                          ),
                    ],
                  ),
                ),
                const Divider(height: 1),
                Expanded(
                  child: filtered.isEmpty
                      ? Center(
                          child: Text(
                            'No records yet — use Sync to pull from the device.',
                            style: Theme.of(context)
                                .textTheme
                                .bodyMedium
                                ?.copyWith(color: NiSenseColors.textMuted),
                          ),
                        )
                      : ListView.builder(
                          itemCount: filtered.length,
                          itemBuilder: (context, i) {
                            final r = filtered[filtered.length - 1 - i]; // newest first
                            return ListTile(
                              dense: true,
                              leading: CircleAvatar(
                                radius: 14,
                                child: Text(
                                  _typeName(r.type).substring(0, 1),
                                  style: const TextStyle(fontSize: 12),
                                ),
                              ),
                              title: Text(_summarize(r)),
                              subtitle: Text(
                                'ID ${r.recordId}  ·  ${_typeName(r.type)}  ·  '
                                'synced ${r.receivedAt.toLocal()}',
                              ),
                            );
                          },
                        ),
                ),
              ],
            ),
    );
  }
}
