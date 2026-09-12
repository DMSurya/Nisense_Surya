import QtQuick
import QtQuick.Controls

// ActionButton — stateful button with busy/success/error feedback.
// States: idle → busy → success → idle  (or error → idle)
// Props:
//   label          : string  — button text in idle state
//   busyLabel      : string  — optional override during busy (defaults to label)
//   accentColor    : color   — base accent (fill tint when idle)
//   busy           : bool    — external busy driver (from backend property)
//   iconText       : string  — optional leading icon (emoji/symbol)
//   outlined       : bool    — use outlined style instead of filled

Rectangle {
    id: root

    property string label:       "Action"
    property string busyLabel:   ""
    property string iconText:    ""
    property color  accentColor: "#3B82F6"
    property bool   busy:        false
    property bool   outlined:    false
    property bool   _internalBusy: false

    signal clicked()

    // ---------------------------------------------------------------
    // Derived state helpers
    readonly property bool _anyBusy: busy || _internalBusy
    readonly property string _displayLabel: {
        if (_anyBusy)    return busyLabel !== "" ? busyLabel : label
        if (_success)    return "✓  " + label
        if (_error)      return "⚠  " + label
        return iconText !== "" ? iconText + "  " + label : label
    }
    property bool _success: false
    property bool _error:   false

    // ---------------------------------------------------------------
    implicitWidth: 140
    implicitHeight: 40

    radius: 8
    color: {
        if (!mouseArea.enabled)  return Qt.rgba(accentColor.r, accentColor.g, accentColor.b, outlined ? 0 : 0.06)
        if (_success)            return Qt.rgba(0.063, 0.725, 0.506, outlined ? 0 : 0.18)
        if (_error)              return Qt.rgba(0.937, 0.267, 0.267, outlined ? 0 : 0.18)
        if (mouseArea.pressed)   return Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.30)
        if (mouseArea.containsMouse) return Qt.rgba(accentColor.r, accentColor.g, accentColor.b, outlined ? 0.10 : 0.22)
        return Qt.rgba(accentColor.r, accentColor.g, accentColor.b, outlined ? 0 : 0.16)
    }
    border.color: {
        if (_success) return "#10B981"
        if (_error)   return "#EF4444"
        return _anyBusy ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.4)
                        : Qt.rgba(accentColor.r, accentColor.g, accentColor.b, outlined ? 0.7 : 0.35)
    }
    border.width: 1

    Behavior on color       { ColorAnimation { duration: 150 } }
    Behavior on border.color{ ColorAnimation { duration: 150 } }
    opacity: mouseArea.enabled ? 1.0 : 0.45
    Behavior on opacity { NumberAnimation { duration: 150 } }

    // ---------------------------------------------------------------
    // Content row: spinner OR label
    Row {
        anchors.centerIn: parent
        spacing: 7

        BusyIndicator {
            id: spinner
            visible: root._anyBusy
            running: root._anyBusy
            width: 18; height: 18
            anchors.verticalCenter: parent.verticalCenter
            palette.dark:  root.accentColor
            palette.mid:   Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.25)
        }

        Text {
            text: root._displayLabel
            color: {
                if (!mouseArea.enabled) return Qt.rgba(0.949, 0.961, 0.976, 0.35)
                if (root._success)      return "#10B981"
                if (root._error)        return "#EF4444"
                return "#F1F5F9"
            }
            font.pixelSize: 14
            font.weight: Font.Normal
            anchors.verticalCenter: parent.verticalCenter
            Behavior on color { ColorAnimation { duration: 150 } }
        }
    }

    // ---------------------------------------------------------------
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: !root._anyBusy
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            root.clicked()
        }
    }

    // ---------------------------------------------------------------
    // Auto-reset timers
    Timer {
        id: successTimer
        interval: 1500
        onTriggered: { root._success = false }
    }
    Timer {
        id: errorTimer
        interval: 1200
        onTriggered: { root._error = false }
    }

    // ---------------------------------------------------------------
    // Public API
    function showSuccess() {
        root._success = true
        root._error   = false
        successTimer.restart()
    }
    function showError() {
        root._error   = true
        root._success = false
        errorTimer.restart()
    }
}

