import 'package:flutter/material.dart';
import 'package:flutter_svg/flutter_svg.dart';

import 'nisense_colors.dart';

/// SVG param/nav icons (from apps/pc) + PNG branding paths.
abstract final class NiSenseIcons {
  static const svgBase = 'assets/svg';
  static const appIcon = 'assets/icons/app.png';
  static const logo = 'assets/icons/icon.png';
  static const splash = 'assets/splash.png';

  static String paramSvg(String key) {
    const map = {
      'hr': 'heart',
      'spo2': 'spo2',
      'glucose': 'glucose',
      'hb': 'hemoglobin',
      'temp': 'temp',
      'resp': 'resp',
      'insulin': 'insulin',
      'homa': 'homa',
      'hrv': 'heart',
      'battery': 'battery',
      'battery_low': 'battery_low',
      'bluetooth': 'bluetooth',
      'wifi': 'wifi',
    };
    final name = map[key] ?? key;
    return '$svgBase/$name.svg';
  }

  static String navSvg(String key, {bool active = false}) =>
      '$svgBase/nav_${key}${active ? '_active' : ''}.svg';

  static Widget paramIcon(String key, {double size = 24, Color? color}) {
    return SvgPicture.asset(
      paramSvg(key),
      width: size,
      height: size,
      colorFilter: ColorFilter.mode(
        color ?? NiSenseColors.paramColor(key),
        BlendMode.srcIn,
      ),
    );
  }

  static Widget navIcon(String key, {bool active = false, double size = 22}) {
    return SvgPicture.asset(
      navSvg(key, active: active),
      width: size,
      height: size,
    );
  }

  static Widget logoImage({double size = 28}) {
    return Image.asset(logo, width: size, height: size, fit: BoxFit.contain);
  }
}
