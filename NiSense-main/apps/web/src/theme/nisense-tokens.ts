// NiSense brand palette — single web source of truth, kept in sync with
// apps/mobile/lib/theme/nisense_colors.dart, apps/pc/qml/Theme.qml and
// src/ui/ui_theme.h. Dark theme.

export const tokens = {
  bg: {
    primary: "#040C24",
    nav: "#070F2A",
    header: "#070F2A",
    elevated: "#0C1A4A",
    card: "#122158",
  },
  brand: {
    primary: "#1E4DFF",
    deepBlue: "#0B1E9A",
    secondary: "#00C8FF",
    health: "#7ED321",
    accentRed: "#FF4A4A",
    accentAmber: "#FF9F1A",
  },
  status: {
    normal: "#00C853",
    warning: "#FFD600",
    risk: "#FF6D00",
    critical: "#D50000",
    info: "#00C8FF",
  },
  param: {
    hr: "#FF4A4A",
    spo2: "#00A6FF",
    glucose: "#FF9F1A",
    hb: "#8A2EFF",
    temp: "#00D46A",
    resp: "#00C8C8",
    hrv: "#4DD2FF",
    insulin: "#2D7FFF",
    homa: "#4DD2FF",
  },
  text: {
    primary: "#F5F8FF",
    muted: "#A8B0D3",
    dim: "#5A6699",
    navActive: "#93C5FD",
  },
  border: {
    subtle: "rgba(255,255,255,0.07)",
    mid: "rgba(30,77,255,0.28)",
  },
} as const;

export function paramColor(key: string): string {
  return (tokens.param as Record<string, string>)[key] ?? tokens.brand.secondary;
}
