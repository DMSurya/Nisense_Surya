import 'package:intl/intl.dart';

/// Converts device unix timestamps for CSV / XLSX export columns.
///
/// Device RTC is synced with **local wall-clock** fields via BLE CTS (and the
/// watch face shows `unix % 86400` with no timezone). Firmware then builds the
/// epoch with `mktime` as if those fields were UTC. So the stored epoch already
/// encodes local civil time — do **not** call [DateTime.toLocal] again or IST
/// (and similar) offsets get applied twice (+5:30 in India).
class RecordTimestamp {
  RecordTimestamp._();

  static final _dateFmt = DateFormat('yyyy-MM-dd');
  static final _timeFmt = DateFormat('HH:mm:ss');
  static final _timeMsFmt = DateFormat('HH:mm:ss.SSS');

  /// Civil time as stored on-device (no second timezone shift).
  static DateTime civilFromUnixSec(int unixSec) =>
      DateTime.fromMillisecondsSinceEpoch(unixSec * 1000, isUtc: true);

  /// Civil time as stored on-device (no second timezone shift).
  static DateTime civilFromUnixMs(int unixMs) =>
      DateTime.fromMillisecondsSinceEpoch(unixMs, isUtc: true);

  /// `yyyy-MM-dd` from device epoch (unix seconds).
  static String date(int unixSec) => _dateFmt.format(civilFromUnixSec(unixSec));

  /// `HH:mm:ss` from device epoch (unix seconds, 24-hour).
  static String time(int unixSec) => _timeFmt.format(civilFromUnixSec(unixSec));

  /// `yyyy-MM-dd` from device epoch (unix milliseconds).
  static String dateMs(int unixMs) => _dateFmt.format(civilFromUnixMs(unixMs));

  /// `HH:mm:ss.SSS` from device epoch (unix milliseconds).
  static String timeMs(int unixMs) =>
      _timeMsFmt.format(civilFromUnixMs(unixMs));
}
