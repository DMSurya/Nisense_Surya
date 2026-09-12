import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: chartsPage
    background: Rectangle { color: "#0A0E1A" }

    // Rolling buffers
    property var hrPoints:           []
    property var spo2Points:         []
    property var glucosePoints:      []
    property var glucoseSamplePoints:[]
    property var ppgIrPoints:        []
    property var ppgRedPoints:       []
    property int ppgSampleIdx:       0
    property real glucoseWindowSec:  86400   // 24 h

    Connections {
        target: backend

        function onHrUpdated(bpm, conf) {
            var t = Date.now() / 1000
            hrPoints.push({ x: t, y: bpm })
            if (hrPoints.length > 300) hrPoints.shift()
            hrCanvas.requestPaint()
        }

        function onSpo2Updated(pct, conf) {
            var t = Date.now() / 1000
            spo2Points.push({ x: t, y: pct })
            if (spo2Points.length > 300) spo2Points.shift()
            spo2Canvas.requestPaint()
        }

        function onGlucoseUpdated(mgdl, mmol, quality) {
            var t = Date.now() / 1000
            glucosePoints.push({ x: t, y: mgdl })
            if (glucosePoints.length > 300) glucosePoints.shift()
            glucoseCanvas.requestPaint()
        }

        function onGlucoseSampleUpdated(sampleNumber, totalSamples, voltageMv, rawAdcValue, timestamp) {
            glucoseSamplePoints.push({ x: sampleNumber, y: voltageMv, total: totalSamples })
            if (glucoseSamplePoints.length > 300) glucoseSamplePoints.shift()
            glucoseLiveCanvas.requestPaint()
        }

        function onMeasStatusUpdated(active, type, pct, quality) {
            if (!active && type === 3) {
                glucoseSamplePoints = []
                glucoseLiveCanvas.requestPaint()
            }
        }

        function onPpgSampleReceived(snum, ir, red, green, ax, ay, az, ts) {
            ppgIrPoints.push({ x: ppgSampleIdx, y: ir })
            ppgRedPoints.push({ x: ppgSampleIdx, y: red })
            ppgSampleIdx++
            if (ppgIrPoints.length > 330) { ppgIrPoints.shift(); ppgRedPoints.shift() }
            ppgCanvas.requestPaint()
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    ColumnLayout {
        x: 24; y: 24
        width: parent.width - 48
        spacing: 16

        // Page title
        Text { text: "Live Charts"; color: "#F1F5F9"; font.pixelSize: 22; font.bold: true }

        // ---------------------------------------------------------------
        // Custom tab bar — underline style
        // ---------------------------------------------------------------
        Item {
            Layout.fillWidth: true; height: 38

            Row {
                id: tabRow; spacing: 0

                Repeater {
                    model: ["Cardio", "Glucose", "PPG"]
                    delegate: Item {
                        width: Math.max(tabLbl.implicitWidth || 60, 60) + 32; height: 38

                        Text {
                            id: tabLbl
                            anchors.centerIn: parent
                            text: modelData
                            color: chartTabs.currentIndex === index ? "#93C5FD" : "#64748B"
                            font.pixelSize: 14
                            font.bold: chartTabs.currentIndex === index
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        // Active underline
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: chartTabs.currentIndex === index ? Math.max(tabLbl.implicitWidth || 60, 60) + 8 : 0
                            height: 2; radius: 1; color: "#3B82F6"
                            Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                        }

                        HoverHandler { id: tabHover }
                        TapHandler  { onTapped: chartTabs.currentIndex = index }
                    }
                }
            }

            // Full-width bottom separator
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width; height: 1
                color: Qt.rgba(1,1,1,0.06)
            }

            // Hidden StackLayout controller
            StackLayout { id: chartTabs; visible: false }
        }

        StackLayout {
            Layout.fillWidth: true
            implicitHeight: 440
            currentIndex: chartTabs.currentIndex

            // -------------------------------------------------------
            // Cardio tab
            // -------------------------------------------------------
            ColumnLayout {
                spacing: 10

                // HR chart
                ChartCard {
                    title: "Heart Rate"
                    unit: "BPM"
                    accentColor: "#EF4444"
                    currentValue: chartsPage.hrPoints.length > 0
                                  ? chartsPage.hrPoints[chartsPage.hrPoints.length-1].y
                                  : 0
                    Layout.fillWidth: true; Layout.preferredHeight: 200

                    Canvas {
                        id: hrCanvas; anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            chartsPage._drawLineChart(ctx, chartsPage.hrPoints, "#EF4444", 40, 200, width, height)
                            chartsPage._drawYLabels(ctx, 40, 200, width, height, "#EF4444")
                        }
                    }
                }

                // SpO2 chart
                ChartCard {
                    title: "SpO₂"
                    unit: "%"
                    accentColor: "#3B82F6"
                    currentValue: chartsPage.spo2Points.length > 0
                                  ? chartsPage.spo2Points[chartsPage.spo2Points.length-1].y
                                  : 0
                    Layout.fillWidth: true; Layout.preferredHeight: 200

                    Canvas {
                        id: spo2Canvas; anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            chartsPage._drawLineChart(ctx, chartsPage.spo2Points, "#3B82F6", 85, 100, width, height)
                            chartsPage._drawYLabels(ctx, 85, 100, width, height, "#3B82F6")
                        }
                    }
                }
            }

            // -------------------------------------------------------
            // Glucose tab
            // -------------------------------------------------------
            ColumnLayout {
                spacing: 10

                // Live sampling
                ChartCard {
                    id: liveCard
                    title: "Glucose Live Sampling"
                    unit: "mV"
                    accentColor: "#06B6D4"
                    currentValue: chartsPage.glucoseSamplePoints.length > 0
                                  ? chartsPage.glucoseSamplePoints[chartsPage.glucoseSamplePoints.length-1].y
                                  : 0
                    Layout.fillWidth: true; Layout.preferredHeight: 200

                    // Sample counter badge
                    Rectangle {
                        anchors { right: parent.right; top: parent.top; margins: 8 }
                        height: 20; width: Math.max(cntLbl.implicitWidth || 50, 50) + 12
                        radius: 10
                        color: Qt.rgba(0.024,0.714,0.831,0.18)
                        border.color: Qt.rgba(0.024,0.714,0.831,0.40)
                        visible: chartsPage.glucoseSamplePoints.length > 0

                        Text {
                            id: cntLbl
                            anchors.centerIn: parent
                            text: {
                                var pts = chartsPage.glucoseSamplePoints
                                if (pts.length === 0) return ""
                                var last = pts[pts.length-1]
                                return pts.length + "/" + (last.total || "?") + " samples"
                            }
                            color: "#67E8F9"; font.pixelSize: 10
                        }
                    }

                    // Sample progress bar
                    Rectangle {
                        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                        height: 2; color: Qt.rgba(1,1,1,0.06)
                        visible: chartsPage.glucoseSamplePoints.length > 0

                        Rectangle {
                            height: parent.height; color: "#06B6D4"; radius: 1
                            width: {
                                var pts = chartsPage.glucoseSamplePoints
                                if (pts.length === 0) return 0
                                var last = pts[pts.length-1]
                                return last.total > 0 ? parent.width * pts.length / last.total : 0
                            }
                            Behavior on width { NumberAnimation { duration: 200 } }
                        }
                    }

                    Canvas {
                        id: glucoseLiveCanvas; anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            if (chartsPage.glucoseSamplePoints.length === 0) {
                                chartsPage._drawEmptyState(ctx, width, height, "No active glucose run")
                                return
                            }
                            var vals = chartsPage.glucoseSamplePoints.map(function(p) { return p.y })
                            var vMin = Math.min.apply(null, vals)
                            var vMax = Math.max.apply(null, vals)
                            var pad2 = Math.max(1, (vMax - vMin) * 0.1)
                            chartsPage._drawLineChartIdx(ctx, chartsPage.glucoseSamplePoints, "#06B6D4", vMin-pad2, vMax+pad2, width, height)
                            chartsPage._drawYLabels(ctx, vMin-pad2, vMax+pad2, width, height, "#06B6D4")
                        }
                    }
                }

                // History
                ChartCard {
                    title: "Glucose History (24 h)"
                    unit: "mg/dL"
                    accentColor: "#F59E0B"
                    currentValue: chartsPage.glucosePoints.length > 0
                                  ? chartsPage.glucosePoints[chartsPage.glucosePoints.length-1].y
                                  : 0
                    Layout.fillWidth: true; Layout.preferredHeight: 200

                    Canvas {
                        id: glucoseCanvas; anchors.fill: parent
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var now = Date.now() / 1000
                            var xMin = now - chartsPage.glucoseWindowSec
                            var filtered = chartsPage.glucosePoints.filter(function(p) { return p.x >= xMin })
                            if (filtered.length === 0) {
                                chartsPage._drawEmptyState(ctx, width, height, "No glucose history yet")
                                return
                            }
                            var values = filtered.map(function(p) { return p.y })
                            var vMin = Math.max(40, Math.min.apply(null, values) - 5)
                            var vMax = Math.max.apply(null, values) + 5
                            chartsPage._drawLineChartWindow(ctx, filtered, "#F59E0B", xMin, now, vMin, vMax, width, height)
                            chartsPage._drawYLabels(ctx, vMin, vMax, width, height, "#F59E0B")
                        }
                    }
                }
            }

            // -------------------------------------------------------
            // PPG tab
            // -------------------------------------------------------
            ChartCard {
                title: "PPG Waveform (rolling 10 s)"
                unit: "raw"
                accentColor: "#8B5CF6"
                currentValue: 0
                showCurrentValue: false
                Layout.fillWidth: true; implicitHeight: 420

                Canvas {
                    id: ppgCanvas; anchors.fill: parent
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        if (chartsPage.ppgIrPoints.length < 2) {
                            chartsPage._drawEmptyState(ctx, width, height, "Waiting for PPG data…")
                            return
                        }
                        var allY = chartsPage.ppgIrPoints.map(function(p){return p.y})
                                   .concat(chartsPage.ppgRedPoints.map(function(p){return p.y}))
                        var yMin = Math.min.apply(null, allY)
                        var yMax = Math.max.apply(null, allY)
                        if (yMax === yMin) yMax = yMin + 1
                        chartsPage._drawLineChartIdx(ctx, chartsPage.ppgIrPoints, "#EF4444", yMin, yMax, width, height)
                        chartsPage._drawLineChartIdx(ctx, chartsPage.ppgRedPoints, "#F97316", yMin, yMax, width, height)
                        // Legend
                        ctx.font = "bold 11px sans-serif"
                        ctx.fillStyle = "#EF4444"; ctx.fillText("▬ IR",  10, 18)
                        ctx.fillStyle = "#F97316"; ctx.fillText("▬ Red", 50, 18)
                        chartsPage._drawYLabels(ctx, yMin, yMax, width, height, "#8B5CF6")
                    }
                }
            }
        }
    }   // ColumnLayout
    }   // ScrollView

    // -----------------------------------------------------------------------
    // Inline ChartCard component
    // -----------------------------------------------------------------------
    component ChartCard: Rectangle {
        id: cc
        property string title:             ""
        property string unit:              ""
        property color  accentColor:       "#3B82F6"
        property real   currentValue:      0
        property bool   showCurrentValue:  true
        default property alias chartContent: chartSlot.data

        radius: 12
        color: Qt.rgba(1,1,1,0.038)
        border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.20)
        clip: true

        // Header bar
        Rectangle {
            id: ccHeader
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 32; color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.08)

            RowLayout {
                anchors { fill: parent; leftMargin: 12; rightMargin: 10 }
                Text { text: cc.title; color: "#94A3B8"; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                // Current value overlay (top-right)
                Text {
                    visible: cc.showCurrentValue && cc.currentValue > 0
                    text: cc.currentValue.toFixed(0) + " " + cc.unit
                    color: cc.accentColor
                    font.pixelSize: 12; font.bold: true
                }
            }
        }

        Item {
            id: chartSlot
            anchors { top: ccHeader.bottom; left: parent.left; right: parent.right; bottom: parent.bottom }
            anchors.margins: 4
        }
    }

    // -----------------------------------------------------------------------
    // Chart drawing helpers
    // -----------------------------------------------------------------------
    function _drawLineChart(ctx, points, color, yMin, yMax, w, h) {
        var pad = 6
        _drawGrid(ctx, w, h, pad)
        if (points.length < 1) return
        var xMin = points[0].x
        var xMax = points[points.length - 1].x
        if (points.length === 1 || xMax === xMin) {
            ctx.fillStyle = color; ctx.beginPath()
            ctx.arc(w/2, h/2, 3, 0, Math.PI*2); ctx.fill(); return
        }
        ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.beginPath()
        for (var i = 0; i < points.length; i++) {
            var px = pad + (points[i].x - xMin) / (xMax - xMin) * (w - 2*pad)
            var py = h - pad - (points[i].y - yMin) / (yMax - yMin) * (h - 2*pad)
            if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py)
        }
        ctx.stroke()
    }

    function _drawLineChartWindow(ctx, points, color, xMin, xMax, yMin, yMax, w, h) {
        var pad = 6
        _drawGrid(ctx, w, h, pad)
        if (points.length < 1 || xMax <= xMin) return
        if (points.length === 1) {
            ctx.fillStyle = color; ctx.beginPath()
            var p0x = pad + (points[0].x - xMin)/(xMax-xMin)*(w-2*pad)
            var p0y = h - pad - (points[0].y - yMin)/(yMax-yMin)*(h-2*pad)
            ctx.arc(p0x, p0y, 3, 0, Math.PI*2); ctx.fill(); return
        }
        ctx.strokeStyle = color; ctx.lineWidth = 2; ctx.beginPath()
        for (var i = 0; i < points.length; i++) {
            var px = pad + (points[i].x - xMin) / (xMax - xMin) * (w - 2*pad)
            var py = h - pad - (points[i].y - yMin) / (yMax - yMin) * (h - 2*pad)
            if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py)
        }
        ctx.stroke()
    }

    function _drawLineChartIdx(ctx, points, color, yMin, yMax, w, h) {
        var pad = 6
        _drawGrid(ctx, w, h, pad)
        if (points.length < 2) return
        var n = points.length
        ctx.strokeStyle = color; ctx.lineWidth = 1.8; ctx.beginPath()
        for (var i = 0; i < n; i++) {
            var px = pad + i / (n-1) * (w - 2*pad)
            var py = h - pad - (points[i].y - yMin) / (yMax - yMin) * (h - 2*pad)
            if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py)
        }
        ctx.stroke()
    }

    function _drawGrid(ctx, w, h, pad) {
        ctx.strokeStyle = Qt.rgba(1,1,1,0.06); ctx.lineWidth = 1
        for (var i = 1; i < 4; i++) {
            var y = pad + i/4 * (h - 2*pad)
            ctx.beginPath(); ctx.moveTo(pad, y); ctx.lineTo(w-pad, y); ctx.stroke()
            var x = pad + i/4 * (w - 2*pad)
            ctx.beginPath(); ctx.moveTo(x, pad); ctx.lineTo(x, h-pad); ctx.stroke()
        }
    }

    function _drawYLabels(ctx, yMin, yMax, w, h, color) {
        var pad = 6
        ctx.font = "10px sans-serif"; ctx.fillStyle = color
        ctx.globalAlpha = 0.65
        ctx.fillText(yMax.toFixed(0), w - pad - 24, pad + 12)
        ctx.fillText(yMin.toFixed(0), w - pad - 24, h - pad - 4)
        ctx.globalAlpha = 1.0
    }

    function _drawEmptyState(ctx, w, h, msg) {
        ctx.font = "13px sans-serif"
        ctx.fillStyle = Qt.rgba(1,1,1,0.20)
        ctx.textAlign = "center"
        ctx.fillText(msg, w/2, h/2)
        ctx.textAlign = "start"
    }
}

