pragma Singleton
import QtQuick

// =============================================================================
// NiSense brand theme — single source of truth for the HCM monitor app.
//
// Kept in lock-step with the firmware brand system in src/ui/ui_theme.h (NS_RGB_*)
// so the watch and the desktop app read as ONE product: deep medical blue +
// health green dominate; red/orange/yellow are reserved for medical severity.
//
// Per-parameter colours (paramHr, paramSpo2, …) are byte-for-byte identical to
// the firmware so a value's colour identity is consistent across both screens
// and every chart.
// =============================================================================
QtObject {
    // ---------------------------------------------------------------
    // Background layers (brand navy, from NS_RGB_BG 0x040C24)
    // ---------------------------------------------------------------
    readonly property color bgPrimary:       "#040C24"
    readonly property color bgNav:           "#070F2A"
    readonly property color bgHeader:        "#070F2A"
    readonly property color bgElevated:      "#0C1A4A"   // NS_RGB_CARD-aligned
    readonly property color bgSurface:       Qt.rgba(1, 1, 1, 0.05)
    readonly property color bgSurfaceHover:  Qt.rgba(1, 1, 1, 0.085)
    readonly property color bgSurfaceActive: Qt.rgba(1, 1, 1, 0.13)
    readonly property color bgInput:         Qt.rgba(1, 1, 1, 0.06)
    readonly property color bgOverlay:       Qt.rgba(0.016, 0.047, 0.141, 0.86)

    // ---------------------------------------------------------------
    // Borders
    // ---------------------------------------------------------------
    readonly property color borderSubtle: Qt.rgba(1, 1, 1, 0.07)
    readonly property color borderMid:    Qt.rgba(0.118, 0.302, 1.0, 0.28)  // brand primary tint
    readonly property color borderStrong: Qt.rgba(0.118, 0.302, 1.0, 0.55)

    // ---------------------------------------------------------------
    // Accent palette (brand)
    // ---------------------------------------------------------------
    readonly property color accentBlue:   "#1E4DFF"   // NS_RGB_PRIMARY
    readonly property color accentDeep:    "#0B1E9A"  // NS_RGB_DEEP_BLUE
    readonly property color accentCyan:   "#00C8FF"   // NS_RGB_SECONDARY
    readonly property color accentGreen:  "#00C853"   // NS_RGB_NORMAL
    readonly property color brandGreen:   "#7ED321"   // NS_RGB_HEALTH
    readonly property color accentRed:    "#FF4A4A"
    readonly property color accentAmber:  "#FF9F1A"
    readonly property color accentPurple: "#8A2EFF"

    // dimmed / muted accent variants (track/bg of progress rings etc.)
    readonly property color accentBlueDim:  Qt.rgba(0.118, 0.302, 1.0, 0.18)
    readonly property color accentCyanDim:  Qt.rgba(0.0, 0.784, 1.0, 0.18)
    readonly property color accentGreenDim: Qt.rgba(0.0, 0.784, 0.325, 0.18)
    readonly property color accentRedDim:   Qt.rgba(1.0, 0.290, 0.290, 0.18)
    readonly property color accentAmberDim: Qt.rgba(1.0, 0.624, 0.102, 0.18)

    // ---------------------------------------------------------------
    // Status / severity (use the SAME system as firmware everywhere)
    // ---------------------------------------------------------------
    readonly property color statusNormal:   "#00C853"  // NS_RGB_NORMAL
    readonly property color statusWarning:  "#FFD600"  // NS_RGB_WARNING
    readonly property color statusRisk:     "#FF6D00"  // NS_RGB_RISK
    readonly property color statusCritical: "#D50000"  // NS_RGB_CRITICAL
    readonly property color statusInfo:     "#00C8FF"  // NS_RGB_INFO

    // ---------------------------------------------------------------
    // Per-parameter accent colours (identical to src/ui/ui_theme.h)
    // ---------------------------------------------------------------
    readonly property color paramHr:      "#FF4A4A"   // NS_RGB_HR
    readonly property color paramSpo2:    "#00A6FF"   // NS_RGB_SPO2
    readonly property color paramGlucose: "#FF9F1A"   // NS_RGB_GLUCOSE
    readonly property color paramHb:      "#8A2EFF"   // NS_RGB_HB
    readonly property color paramTemp:    "#00D46A"   // NS_RGB_TEMP
    readonly property color paramResp:    "#00C8C8"   // NS_RGB_RESP
    readonly property color paramHrv:     "#4DD2FF"   // NS_RGB_HRV
    readonly property color paramBpSys:   "#FF4D4D"   // NS_RGB_BP_SYS
    readonly property color paramBpDia:   "#2D7FFF"   // NS_RGB_BP_DIA

    // ---------------------------------------------------------------
    // Text
    // ---------------------------------------------------------------
    readonly property color textPrimary: "#F5F8FF"
    readonly property color textMuted:   "#A8B0D3"   // NS_RGB_TEXT_DIM
    readonly property color textDim:     "#5A6699"

    // Navigation rail tints
    readonly property color navMuted:  "#64748B"
    readonly property color navActive: "#93C5FD"

    // ---------------------------------------------------------------
    // Brand assets
    // ---------------------------------------------------------------
    readonly property url logo: Qt.resolvedUrl("assets/logo/logo.jpeg")

    // Resolve a tuned parameter/state icon (assets/icons/<name>.svg).
    function icon(name) { return Qt.resolvedUrl("assets/icons/" + name + ".svg") }

    // Resolve a navigation icon in muted or active tint.
    function navIcon(name, active) {
        return Qt.resolvedUrl("assets/icons/nav_" + name + (active ? "_active" : "") + ".svg")
    }

    // Map a parameter key to its brand colour (chart series, cards, badges).
    function paramColor(key) {
        switch (key) {
        case "hr":      return paramHr
        case "spo2":    return paramSpo2
        case "glucose": return paramGlucose
        case "hb":      return paramHb
        case "temp":    return paramTemp
        case "resp":    return paramResp
        case "hrv":     return paramHrv
        case "bpsys":   return paramBpSys
        case "bpdia":   return paramBpDia
        default:        return accentCyan
        }
    }

    // ---------------------------------------------------------------
    // Shape
    // ---------------------------------------------------------------
    readonly property int radius:     12
    readonly property int radiusLg:   16
    readonly property int radiusSm:   8
    readonly property int radiusPill: 999

    // ---------------------------------------------------------------
    // Animation
    // ---------------------------------------------------------------
    readonly property int durationFast:   150
    readonly property int durationNormal: 220
    readonly property int durationSlow:   350

    // ---------------------------------------------------------------
    // Typography (pixel sizes)
    // ---------------------------------------------------------------
    readonly property int fontXs:   11
    readonly property int fontSm:   13
    readonly property int fontBase: 15
    readonly property int fontLg:   18
    readonly property int fontXl:   22
    readonly property int font2xl:  28
    readonly property int font3xl:  36
}
