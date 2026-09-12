import QtQuick

// MetricBadge — compact live-value tile used in the Dashboard.
// Shows icon, label, big value + unit. Dims gracefully when no data.
// Props:
//   icon        : string  — emoji icon
//   label       : string  — card title (e.g. "Heart Rate")
//   value       : string  — current value text (e.g. "76")
//   unit        : string  — unit suffix (e.g. "BPM")
//   subtitleText: string  — optional second line below unit
//   accentColor : color   — tint used for icon ring and glow
//   active      : bool    — false → shows "--" dimmed, mutes glow

GlassCard {
    id: root

    property string icon:         "?"
    property string label:        "Metric"
    property string value:        "--"
    property string unit:         ""
    property string subtitleText: ""
    property color  accentColor:  "#3B82F6"
    property bool   active:       false

    glowColor:   active ? accentColor : "transparent"
    glowOpacity: 0.5

    implicitWidth:  160
    implicitHeight: 88

    Behavior on glowOpacity { NumberAnimation { duration: 300 } }

    Row {
        anchors {
            left:           parent.left
            right:          parent.right
            verticalCenter: parent.verticalCenter
            leftMargin:     14
            rightMargin:    12
        }
        spacing: 12

        // Icon circle
        Rectangle {
            width: 42; height: 42; radius: 21
            color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b,
                           root.active ? 0.18 : 0.08)
            anchors.verticalCenter: parent.verticalCenter

            Behavior on color { ColorAnimation { duration: 250 } }

            Text {
                anchors.centerIn: parent
                text:  root.icon
                font.pixelSize: 20
                opacity: root.active ? 1.0 : 0.45
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
        }

        // Values column
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
                text:  root.label
                color: "#94A3B8"
                font.pixelSize: 12
            }

            Row {
                spacing: 4
                Text {
                    id: valueText
                    text:  root.active ? root.value : "--"
                    color: root.active ? "#F1F5F9" : "#475569"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    Behavior on color { ColorAnimation { duration: 200 } }
                }
                Text {
                    text:  root.active ? root.unit : ""
                    color: "#94A3B8"
                    font.pixelSize: 12
                    anchors.bottom: valueText.bottom
                    bottomPadding: 4
                }
            }

            Text {
                visible: root.subtitleText !== "" && root.active
                text:    root.subtitleText
                color:   "#64748B"
                font.pixelSize: 11
            }
        }
    }

    // Pulse animation when active state changes (value arrived)
    SequentialAnimation on scale {
        id: pulseAnim
        running: false
        NumberAnimation { to: 1.04; duration: 80 }
        NumberAnimation { to: 1.0;  duration: 120; easing.type: Easing.OutBack }
    }

    onActiveChanged: { if (active) pulseAnim.restart() }
}
