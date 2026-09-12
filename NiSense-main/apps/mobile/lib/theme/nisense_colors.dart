import 'package:flutter/material.dart';

/// NiSense brand palette — mirrors apps/pc/qml/Theme.qml and src/ui/ui_theme.h.
abstract final class NiSenseColors {
  static const bgPrimary = Color(0xFF040C24);
  static const bgNav = Color(0xFF070F2A);
  static const bgHeader = Color(0xFF070F2A);
  static const bgElevated = Color(0xFF0C1A4A);
  static const bgCard = Color(0xFF122158);

  static const primary = Color(0xFF1E4DFF);
  static const deepBlue = Color(0xFF0B1E9A);
  static const secondary = Color(0xFF00C8FF);
  static const health = Color(0xFF7ED321);
  static const accentRed = Color(0xFFFF4A4A);
  static const accentAmber = Color(0xFFFF9F1A);

  static const statusNormal = Color(0xFF00C853);
  static const statusWarning = Color(0xFFFFD600);
  static const statusRisk = Color(0xFFFF6D00);
  static const statusCritical = Color(0xFFD50000);
  static const statusInfo = Color(0xFF00C8FF);

  static const paramHr = Color(0xFFFF4A4A);
  static const paramSpo2 = Color(0xFF00A6FF);
  static const paramGlucose = Color(0xFFFF9F1A);
  static const paramHb = Color(0xFF8A2EFF);
  static const paramTemp = Color(0xFF00D46A);
  static const paramResp = Color(0xFF00C8C8);
  static const paramHrv = Color(0xFF4DD2FF);
  static const paramInsulin = Color(0xFF2D7FFF);
  static const paramHoma = Color(0xFF4DD2FF);

  static const textPrimary = Color(0xFFF5F8FF);
  static const textMuted = Color(0xFFA8B0D3);
  static const textDim = Color(0xFF5A6699);
  static const navMuted = Color(0xFF64748B);
  static const navActive = Color(0xFF93C5FD);

  static const borderSubtle = Color(0x12FFFFFF);
  static const borderMid = Color(0x471E4DFF);

  static Color paramColor(String key) {
    switch (key) {
      case 'hr':
        return paramHr;
      case 'spo2':
        return paramSpo2;
      case 'glucose':
        return paramGlucose;
      case 'hb':
        return paramHb;
      case 'temp':
        return paramTemp;
      case 'resp':
        return paramResp;
      case 'hrv':
        return paramHrv;
      case 'insulin':
        return paramInsulin;
      case 'homa':
        return paramHoma;
      default:
        return secondary;
    }
  }
}
