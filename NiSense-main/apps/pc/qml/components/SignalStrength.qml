import QtQuick

// SignalStrength — 4-bar RSSI graphic.
// Props:
//   rssi        : int    — dBm value (-100 to -40); values above -40 treated as -40
//   accentColor : color  — filled-bar color

Item {
    id: root

    property int   rssi:        -100
    property color accentColor: "#3B82F6"

    implicitWidth:  26
    implicitHeight: 18

    // Map rssi dBm → bars (0–4)
    readonly property int bars: {
        if (rssi >= -55) return 4
        if (rssi >= -67) return 3
        if (rssi >= -80) return 2
        if (rssi >= -92) return 1
        return 0
    }

    Row {
        anchors.bottom: parent.bottom
        anchors.left:   parent.left
        spacing: 3

        Repeater {
            model: 4
            Rectangle {
                width:  4
                height: (index + 1) * (root.height / 4)
                anchors.bottom: parent.bottom
                radius: 2
                color: index < root.bars
                       ? root.accentColor
                       : Qt.rgba(1, 1, 1, 0.12)
                Behavior on color { ColorAnimation { duration: 200 } }
            }
        }
    }
}
