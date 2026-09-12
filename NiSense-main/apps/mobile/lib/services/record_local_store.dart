import 'dart:async';
import 'dart:typed_data';

import 'package:path/path.dart' as p;
import 'package:path_provider/path_provider.dart';
import 'package:sqflite/sqflite.dart';

import '../ble/record_sync_client.dart';

/// Persistent local store of every record ever synced from a device.
///
/// Accumulates across all sync sessions so the app can show full history
/// (dashboard metric drill-down, "All Records" browser, global export) even
/// after the device has deleted the originals via ACK wipe.
class RecordLocalStore {
  RecordLocalStore._();
  static final RecordLocalStore instance = RecordLocalStore._();

  Database? _db;

  Future<Database> _open() async {
    if (_db != null) return _db!;
    final dir = await getApplicationDocumentsDirectory();
    final path = p.join(dir.path, 'nisense_records.db');
    _db = await openDatabase(
      path,
      version: 2,
      onCreate: (db, version) async {
        await db.execute('''
          CREATE TABLE records (
            device_id TEXT NOT NULL,
            record_id INTEGER NOT NULL,
            type INTEGER NOT NULL,
            parent_id INTEGER NOT NULL,
            measurement_id INTEGER NOT NULL DEFAULT 0,
            payload BLOB NOT NULL,
            received_at INTEGER NOT NULL,
            pushed INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (device_id, record_id)
          )
        ''');
        await db.execute('CREATE INDEX idx_records_type ON records(device_id, type)');
        await db.execute('CREATE INDEX idx_records_pushed ON records(pushed)');
        await db.execute(
          'CREATE INDEX idx_records_meas ON records(device_id, measurement_id)',
        );
      },
      onUpgrade: (db, oldVersion, newVersion) async {
        if (oldVersion < 2) {
          await db.execute(
            'ALTER TABLE records ADD COLUMN measurement_id INTEGER NOT NULL DEFAULT 0',
          );
          await db.execute(
            'CREATE INDEX IF NOT EXISTS idx_records_meas ON records(device_id, measurement_id)',
          );
        }
      },
    );
    return _db!;
  }

  /// Upsert every pulled record for [deviceId]. Safe to call repeatedly —
  /// re-syncing the same record_id updates payload fields but does **not**
  /// reset an existing `pushed = 1` flag.
  Future<void> insertAll(String deviceId, List<SyncedRecord> records) async {
    if (records.isEmpty) return;
    final db = await _open();
    final now = DateTime.now().millisecondsSinceEpoch;
    await db.transaction((txn) async {
      for (final r in records) {
        await txn.rawInsert(
          '''
          INSERT INTO records (device_id, record_id, type, parent_id, measurement_id, payload, received_at, pushed)
          VALUES (?, ?, ?, ?, ?, ?, ?, 0)
          ON CONFLICT(device_id, record_id) DO UPDATE SET
            type = excluded.type,
            parent_id = excluded.parent_id,
            measurement_id = excluded.measurement_id,
            payload = excluded.payload,
            received_at = excluded.received_at
          ''',
          [
            deviceId,
            r.recordId,
            r.type,
            r.parentId,
            r.measurementId,
            r.payload,
            now,
          ],
        );
      }
    });
  }

  /// All persisted records, optionally filtered by device/type/time.
  Future<List<StoredRecord>> getAll({
    String? deviceId,
    int? type,
    DateTime? since,
    DateTime? until,
  }) async {
    final db = await _open();
    final where = <String>[];
    final args = <Object?>[];
    if (deviceId != null) {
      where.add('device_id = ?');
      args.add(deviceId);
    }
    if (type != null) {
      where.add('type = ?');
      args.add(type);
    }
    if (since != null) {
      where.add('received_at >= ?');
      args.add(since.millisecondsSinceEpoch);
    }
    if (until != null) {
      where.add('received_at <= ?');
      args.add(until.millisecondsSinceEpoch);
    }
    final rows = await db.query(
      'records',
      where: where.isEmpty ? null : where.join(' AND '),
      whereArgs: args.isEmpty ? null : args,
      orderBy: 'record_id ASC',
    );
    return rows.map(StoredRecord.fromRow).toList();
  }

  /// Records not yet successfully pushed to the server.
  Future<List<StoredRecord>> getUnpushed({String? deviceId, int limit = 500}) async {
    final db = await _open();
    final where = <String>['pushed = 0'];
    final args = <Object?>[];
    if (deviceId != null) {
      where.add('device_id = ?');
      args.add(deviceId);
    }
    final rows = await db.query(
      'records',
      where: where.join(' AND '),
      whereArgs: args,
      orderBy: 'record_id ASC',
      limit: limit,
    );
    return rows.map(StoredRecord.fromRow).toList();
  }

  Future<int> countUnpushed({String? deviceId}) async {
    final db = await _open();
    final where = <String>['pushed = 0'];
    final args = <Object?>[];
    if (deviceId != null) {
      where.add('device_id = ?');
      args.add(deviceId);
    }
    final rows = await db.query(
      'records',
      columns: ['COUNT(*) as cnt'],
      where: where.join(' AND '),
      whereArgs: args,
    );
    return (rows.first['cnt'] as int?) ?? 0;
  }

  Future<void> markPushed(String deviceId, List<int> recordIds) async {
    if (recordIds.isEmpty) return;
    final db = await _open();
    final batch = db.batch();
    for (final id in recordIds) {
      batch.update(
        'records',
        {'pushed': 1},
        where: 'device_id = ? AND record_id = ?',
        whereArgs: [deviceId, id],
      );
    }
    await batch.commit(noResult: true);
  }

  /// Delete every locally stored record. Does not touch BLE cursors or device NOR.
  Future<void> clearAll() async {
    final db = await _open();
    await db.delete('records');
  }

  /// Per-type row counts, optionally scoped to one device.
  Future<Map<int, int>> countsByType({String? deviceId}) async {
    final db = await _open();
    final rows = await db.query(
      'records',
      columns: ['type', 'COUNT(*) as cnt'],
      where: deviceId != null ? 'device_id = ?' : null,
      whereArgs: deviceId != null ? [deviceId] : null,
      groupBy: 'type',
    );
    return {for (final r in rows) r['type'] as int: r['cnt'] as int};
  }

  Future<int> totalCount({String? deviceId}) async {
    final db = await _open();
    final rows = await db.query(
      'records',
      columns: ['COUNT(*) as cnt'],
      where: deviceId != null ? 'device_id = ?' : null,
      whereArgs: deviceId != null ? [deviceId] : null,
    );
    return (rows.first['cnt'] as int?) ?? 0;
  }

  Future<List<String>> distinctDeviceIds() async {
    final db = await _open();
    final rows = await db.query('records', columns: ['DISTINCT device_id']);
    return rows.map((r) => r['device_id'] as String).toList();
  }
}

/// A record row read back from the local SQLite store.
class StoredRecord {
  StoredRecord({
    required this.deviceId,
    required this.recordId,
    required this.type,
    required this.parentId,
    required this.measurementId,
    required this.payload,
    required this.receivedAt,
    required this.pushed,
  });

  final String deviceId;
  final int recordId;
  final int type;
  final int parentId;
  final int measurementId;
  final Uint8List payload;
  final DateTime receivedAt;
  final bool pushed;

  static StoredRecord fromRow(Map<String, Object?> row) {
    return StoredRecord(
      deviceId: row['device_id'] as String,
      recordId: row['record_id'] as int,
      type: row['type'] as int,
      parentId: row['parent_id'] as int,
      measurementId: (row['measurement_id'] as int?) ?? 0,
      payload: row['payload'] is Uint8List
          ? row['payload'] as Uint8List
          : Uint8List.fromList(row['payload'] as List<int>),
      receivedAt: DateTime.fromMillisecondsSinceEpoch(row['received_at'] as int),
      pushed: (row['pushed'] as int) != 0,
    );
  }

  SyncedRecord toSyncedRecord() => SyncedRecord(
        recordId: recordId,
        type: type,
        parentId: parentId,
        measurementId: measurementId,
        payload: payload,
      );
}
