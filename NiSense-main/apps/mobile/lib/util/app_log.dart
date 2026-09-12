import 'dart:collection';

import 'package:flutter/foundation.dart';

/// Lightweight runtime logger for `flutter run` / `adb logcat`.
///
/// Lines are prefixed `[NiSense/<tag>]` so they are easy to filter:
/// `adb logcat | Select-String NiSense`
class AppLog {
  AppLog._();

  static const int ringCapacity = 200;
  static final ListQueue<String> _ring = ListQueue<String>(ringCapacity);
  static final List<void Function(String line)> _listeners = [];

  /// Recent lines (oldest → newest) for an in-app log viewer.
  static List<String> get recent => List.unmodifiable(_ring);

  static void addListener(void Function(String line) listener) {
    _listeners.add(listener);
  }

  static void removeListener(void Function(String line) listener) {
    _listeners.remove(listener);
  }

  static void d(String tag, String message) => _emit('D', tag, message);

  static void i(String tag, String message) => _emit('I', tag, message);

  static void w(String tag, String message, [Object? error]) =>
      _emit('W', tag, message, error: error);

  static void e(String tag, String message, [Object? error, StackTrace? stack]) =>
      _emit('E', tag, message, error: error, stack: stack);

  static void _emit(
    String level,
    String tag,
    String message, {
    Object? error,
    StackTrace? stack,
  }) {
    final ts = DateTime.now().toIso8601String().substring(11, 23);
    final line = '[$ts][NiSense/$tag][$level] $message';
    // ignore: avoid_print — must always reach flutter run / logcat
    print(line);
    if (error != null) {
      // ignore: avoid_print
      print('[$ts][NiSense/$tag][$level] cause: $error');
    }
    if (stack != null && kDebugMode) {
      // ignore: avoid_print
      print(stack);
    }

    while (_ring.length >= ringCapacity) {
      _ring.removeFirst();
    }
    _ring.addLast(line);
    if (error != null) {
      _ring.addLast('[$ts][NiSense/$tag][$level] cause: $error');
    }
    for (final listener in List<void Function(String)>.from(_listeners)) {
      try {
        listener(line);
      } catch (_) {}
    }
  }
}
