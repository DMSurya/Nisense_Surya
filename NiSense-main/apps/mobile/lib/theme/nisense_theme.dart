import 'package:flutter/material.dart';

import 'nisense_colors.dart';

abstract final class NiSenseTheme {
  static ThemeData dark() {
    const scheme = ColorScheme.dark(
      primary: NiSenseColors.primary,
      secondary: NiSenseColors.secondary,
      surface: NiSenseColors.bgElevated,
      error: NiSenseColors.statusCritical,
      onPrimary: NiSenseColors.textPrimary,
      onSecondary: NiSenseColors.textPrimary,
      onSurface: NiSenseColors.textPrimary,
      onError: NiSenseColors.textPrimary,
    );

    return ThemeData(
      useMaterial3: true,
      brightness: Brightness.dark,
      colorScheme: scheme,
      scaffoldBackgroundColor: NiSenseColors.bgPrimary,
      appBarTheme: const AppBarTheme(
        backgroundColor: NiSenseColors.bgHeader,
        foregroundColor: NiSenseColors.textPrimary,
        elevation: 0,
        centerTitle: false,
      ),
      navigationBarTheme: NavigationBarThemeData(
        backgroundColor: NiSenseColors.bgNav,
        indicatorColor: NiSenseColors.primary.withValues(alpha: 0.2),
        labelTextStyle: WidgetStateProperty.resolveWith((states) {
          final active = states.contains(WidgetState.selected);
          return TextStyle(
            fontSize: 11,
            color: active ? NiSenseColors.navActive : NiSenseColors.navMuted,
          );
        }),
      ),
      cardTheme: CardThemeData(
        color: NiSenseColors.bgCard,
        elevation: 0,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(12),
          side: const BorderSide(color: NiSenseColors.borderMid),
        ),
      ),
      inputDecorationTheme: InputDecorationTheme(
        filled: true,
        fillColor: NiSenseColors.bgElevated,
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(8),
          borderSide: const BorderSide(color: NiSenseColors.borderSubtle),
        ),
        labelStyle: const TextStyle(color: NiSenseColors.textMuted),
      ),
      snackBarTheme: const SnackBarThemeData(
        backgroundColor: NiSenseColors.bgElevated,
        contentTextStyle: TextStyle(color: NiSenseColors.textPrimary),
      ),
      textTheme: const TextTheme(
        bodyMedium: TextStyle(color: NiSenseColors.textPrimary, fontSize: 15),
        bodySmall: TextStyle(color: NiSenseColors.textMuted, fontSize: 13),
        titleMedium: TextStyle(
          color: NiSenseColors.textPrimary,
          fontSize: 18,
          fontWeight: FontWeight.w600,
        ),
        titleLarge: TextStyle(
          color: NiSenseColors.textPrimary,
          fontSize: 22,
          fontWeight: FontWeight.w600,
        ),
      ),
    );
  }
}
