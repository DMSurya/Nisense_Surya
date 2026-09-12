import QtQuick

// GlassCard — standard glass-morphism surface used throughout the app.
// Props:
//   glowColor  : color  — optional accent border glow (set to transparent for none)
//   glowOpacity: real   — glow strength 0–1 (default 0.6)
//   interactive: bool   — shows hover highlight when true

Rectangle {
    id: root

    property color glowColor:   "transparent"
    property real  glowOpacity: 0.6
    property bool  interactive: false

    color:  Qt.rgba(1, 1, 1, interactive && hoverHandler.hovered ? 0.065 : 0.042)
    radius: 12
    border.color: glowColor !== Qt.rgba(0,0,0,0) && glowColor !== "transparent"
                  ? Qt.rgba(glowColor.r, glowColor.g, glowColor.b, glowOpacity * 0.8)
                  : Qt.rgba(1, 1, 1, 0.075)
    border.width: glowColor !== Qt.rgba(0,0,0,0) && glowColor !== "transparent" ? 1.5 : 1

    Behavior on color        { ColorAnimation { duration: 180 } }
    Behavior on border.color { ColorAnimation { duration: 180 } }

    // Outer glow rectangle (visible only when glowColor is set)
    Rectangle {
        visible: root.glowColor !== Qt.rgba(0,0,0,0) && root.glowColor !== "transparent"
        anchors.fill: parent
        anchors.margins: -3
        radius: root.radius + 3
        color: "transparent"
        border.color: Qt.rgba(root.glowColor.r, root.glowColor.g, root.glowColor.b, root.glowOpacity * 0.25)
        border.width: 3
        z: -1
    }

    HoverHandler {
        id: hoverHandler
        enabled: root.interactive
    }
}
