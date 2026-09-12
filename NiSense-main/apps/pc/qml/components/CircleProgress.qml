import QtQuick

// CircleProgress — Canvas-drawn circular progress ring.
// Props:
//   progress   : real   — 0.0 to 1.0
//   size       : int    — outer diameter
//   ringWidth  : int    — stroke width of ring
//   ringColor  : color  — filled arc color
//   trackColor : color  — background track color
//   label      : string — center text (empty = show percentage)
//   fontSize   : int    — center text size

Item {
    id: root

    property real   progress:   0.0
    property int    size:       56
    property int    ringWidth:  5
    property color  ringColor:  "#3B82F6"
    property color  trackColor: Qt.rgba(1,1,1,0.08)
    property string label:      ""
    property int    fontSize:   13

    implicitWidth:  size
    implicitHeight: size

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var cx  = width  / 2
            var cy  = height / 2
            var r   = Math.min(width, height) / 2 - root.ringWidth / 2

            // Track
            ctx.beginPath()
            ctx.arc(cx, cy, r, 0, Math.PI * 2)
            ctx.strokeStyle = root.trackColor.toString()
            ctx.lineWidth   = root.ringWidth
            ctx.stroke()

            // Progress arc (-90° = 12 o'clock)
            if (root.progress > 0) {
                var startAngle = -Math.PI / 2
                var endAngle   = startAngle + Math.PI * 2 * Math.min(root.progress, 1.0)
                ctx.beginPath()
                ctx.arc(cx, cy, r, startAngle, endAngle)
                ctx.strokeStyle = root.ringColor.toString()
                ctx.lineWidth   = root.ringWidth
                ctx.lineCap     = "round"
                ctx.stroke()
            }
        }

        Connections {
            target: root
            function onProgressChanged()   { canvas.requestPaint() }
            function onRingColorChanged()  { canvas.requestPaint() }
            function onTrackColorChanged() { canvas.requestPaint() }
        }
    }

    Text {
        anchors.centerIn: parent
        text: root.label !== "" ? root.label : Math.round(root.progress * 100) + "%"
        color: "#F1F5F9"
        font.pixelSize: root.fontSize
        font.weight: Font.Normal
        visible: root.progress > 0 || root.label !== ""
    }
}

