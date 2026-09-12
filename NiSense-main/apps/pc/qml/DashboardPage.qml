import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: dashPage
    background: Rectangle { color: "#0A0E1A" }

    // -----------------------------------------------------------------------
    // Live data properties, populated via BLE signals
    // -----------------------------------------------------------------------
    property int  hrBpm:       0
    property int  hrConf:      0
    property int  spo2Pct:     0
    property int  spo2Conf:    0
    property real hbGdl:       0.0
    property int  hbConf:      0
    property int  respBpm:     0
    property int  respConf:    0
    property int  glucoseMgDl: 0
    property real glucoseMmol: 0.0
    property real tempC:       0.0
    property int  battMv:      0
    property int  battCurrentMa: 0
    property int  battSoc:     0
    property int  chargerSt:   0
    property int  chgVmV:      0
    property int  chgImA:      0
    property int  battTempC:   0
    property int  battCycles:  0
    property int  avgCurrentMa: 0
    property int  buck1En:     0
    property int  buck2En:     0
    property int  buck3En:     0
    property int  bboutEn:     0
    property int  buck3Mv:     0
    property int  bboutMv:     0
    property bool measActive:  false
    property int  measType:    0    // 1=HR 2=SpO2 3=Glucose 4=Vitals
    property int  measPct:     0
    property int  measQuality: 0
    property int  accelX:      0
    property int  accelY:      0
    property int  accelZ:      0
    property bool accelActive: false
    property bool wearContact: false
    property int  wearState:     0
    property int  proxRaw:       0
    property int  proxFilt:      0

    readonly property string wearStateLabel: {
        switch (wearState) {
        case 1: return "Loose"
        case 2: return "Good fit"
        default: return wearContact ? "Contact" : "Not worn"
        }
    }

    // PMIC alert thresholds for quick visual health state on dashboard
    property int lowBatterySocPct: 20
    property int lowBatteryMv: 3400
    property int hotBatteryTempC: 45
    property int highCurrentAbsMa: 1200
    property bool isBatteryLow: (battSoc > 0 && battSoc <= lowBatterySocPct) || (battMv > 0 && battMv <= lowBatteryMv)
    property bool isBatteryHot: battTempC >= hotBatteryTempC
    property bool isCurrentHigh: Math.abs(battCurrentMa) >= highCurrentAbsMa

    Connections {
        target: backend
        function onHrUpdated(bpm, conf)          { hrBpm = bpm; hrConf = conf }
        function onSpo2Updated(pct, conf)         { spo2Pct = pct; spo2Conf = conf }
        function onHbUpdated(gdl, conf)           { hbGdl = gdl; hbConf = conf }
        function onRespRateUpdated(bpm, conf)     { respBpm = bpm; respConf = conf }
        function onGlucoseUpdated(mgdl, mmol, q) { glucoseMgDl = mgdl; glucoseMmol = mmol }
        function onTempUpdated(c)                 { tempC = c }
        function onPmicUpdated(mv, ma, soc, chg) {
            battMv = mv
            battCurrentMa = ma
            battSoc = soc
            chargerSt = chg
        }
        function onPmicExtUpdated(b3mv, bbmv, b1en, b2en, b3en, bben, chargeVmV, chargeImA,
                                   batteryTempC, cycles, remMah, fullMah, designMah, tteMin, ttfMin,
                                   avgMa) {
            buck3Mv = b3mv
            bboutMv = bbmv
            buck1En = b1en
            buck2En = b2en
            buck3En = b3en
            bboutEn = bben
            chgVmV = chargeVmV
            chgImA = chargeImA
            battTempC = batteryTempC
            battCycles = cycles
            avgCurrentMa = avgMa
        }
        function onMeasStatusUpdated(active, type, pct, qual) {
            measActive  = active
            measType    = type
            measPct     = pct
            measQuality = qual
        }
        function onAccelUpdated(x, y, z, ts) {
            accelX = x; accelY = y; accelZ = z; accelActive = true
        }
        function onProximityUpdated(contact, state, raw, filt, ts) {
            wearContact = contact !== 0
            wearState = state
            proxRaw = raw
            proxFilt = filt
        }
    }

    // -----------------------------------------------------------------------
    // Layout
    // -----------------------------------------------------------------------
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

    ColumnLayout {
        x: 24; y: 24
        width: parent.width - 48
        spacing: 20

        // Page title
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "Dashboard"
                color: "#F1F5F9"
                font.pixelSize: 22; font.bold: true
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                visible: wearContact || proxRaw > 0
                radius: 10
                color: wearContact ? "#064E3B" : "#1E293B"
                border.color: wearContact ? "#10B981" : "#475569"
                Layout.preferredHeight: 28
                Layout.preferredWidth: wearBadgeRow.implicitWidth + 16

                RowLayout {
                    id: wearBadgeRow
                    anchors.centerIn: parent
                    spacing: 6

                    Rectangle {
                        width: 8; height: 8; radius: 4
                        color: wearContact ? "#10B981" : "#64748B"
                    }
                    Text {
                        text: wearStateLabel
                        color: wearContact ? "#6EE7B7" : "#94A3B8"
                        font.pixelSize: 11
                    }
                    Text {
                        visible: proxFilt > 0
                        text: proxFilt + ""
                        color: "#64748B"
                        font.pixelSize: 10
                    }
                }
            }
        }

        // Secure profile: health streams require MITM pairing
        Rectangle {
            visible: backend && backend.connected && backend.needsPairingForHealth
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            radius: 10
            color: "#422006"
            border.color: "#F59E0B"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: "Pair to view health data — confirm the code on the HCM screen"
                    color: "#FCD34D"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Button {
                    text: "Pair"
                    enabled: backend && backend.connected && !backend.isConnecting
                    onClicked: if (backend) backend.pairDevice()
                }
            }
        }

        // ---------------------------------------------------------------
        // Metric tiles — 3+2 row layout
        // ---------------------------------------------------------------
        RowLayout {

            Repeater {
                model: [
                    { label: "Heart Rate",   value: hrBpm > 0 ? hrBpm + "" : "--",   unit: "BPM",   sub: hrConf > 0 ? "conf " + hrConf + "%" : "",    accent: "#EF4444",  icon: "♥"  },
                    { label: "SpO₂",         value: spo2Pct > 0 ? spo2Pct + "" : "--", unit: "%",   sub: spo2Conf > 0 ? "conf " + spo2Conf + "%" : "", accent: "#3B82F6",  icon: "○"  },
                    { label: "Glucose",      value: glucoseMgDl > 0 ? glucoseMgDl + "" : "--", unit: "mg/dL", sub: glucoseMmol > 0 ? glucoseMmol.toFixed(1) + " mmol/L" : "", accent: "#F59E0B", icon: "◆" },
                ]
                delegate: MetricCard {
                    Layout.fillWidth: true
                    label: modelData.label
                    valueText: modelData.value
                    unit: modelData.unit
                    subText: modelData.sub
                    accent: modelData.accent
                    iconText: modelData.icon
                    active: modelData.value !== "--"
                }
            }
        }

        RowLayout {
            spacing: 12; Layout.fillWidth: true

            Repeater {
                model: [
                    { label: "Temperature", value: tempC > 0 ? tempC.toFixed(1) : "--", unit: "°C",  sub: "",       accent: "#06B6D4", icon: "◉" },
                    { label: "Battery",     value: battSoc > 0 ? battSoc + "" : "--",   unit: "%",   sub: battMv > 0 ? battMv + " mV" : "", accent: "#10B981", icon: "▮" },
                ]
                delegate: MetricCard {
                    Layout.fillWidth: true
                    label: modelData.label
                    valueText: modelData.value
                    unit: modelData.unit
                    subText: modelData.sub
                    accent: modelData.accent
                    iconText: modelData.icon
                    active: modelData.value !== "--"
                }
            }

            // Charger status
            Rectangle {
                Layout.fillWidth: true; height: 90; radius: 12
                color: Qt.rgba(1,1,1,0.04); border.color: Qt.rgba(1,1,1,0.08)
                Column {
                    anchors.centerIn: parent; spacing: 4
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: chargerSt === 1 ? "⚡" : chargerSt === 2 ? "✓" : "—"; font.pixelSize: 22 }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: chargerSt === 1 ? "Charging" : chargerSt === 2 ? "Full" : "Discharging"
                        color: chargerSt === 1 ? "#FBBF24" : chargerSt === 2 ? "#10B981" : "#94A3B8"
                        font.pixelSize: 13; font.bold: false
                    }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Charger"; color: "#475569"; font.pixelSize: 11 }
                }
            }
        }

        // ---------------------------------------------------------------
        // Measurement control card
        // ---------------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            height: measPanel.implicitHeight + 32
            radius: 14
            color: Qt.rgba(1,1,1,0.04)
            border.color: measActive
                          ? Qt.rgba(0.231, 0.510, 0.965, 0.28)
                          : Qt.rgba(1,1,1,0.07)
            Behavior on border.color { ColorAnimation { duration: 250 } }

            ColumnLayout {
                id: measPanel
                anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: 16; leftMargin: 20; rightMargin: 20 }
                spacing: 14

                // Header
                RowLayout {
                    Text { text: "Measurement"; color: "#F1F5F9"; font.pixelSize: 15; font.bold: true }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        height: 22
                        radius: 11
                        color: backend && backend.csvWriteBlocked
                               ? Qt.rgba(0.937, 0.267, 0.267, 0.16)
                               : Qt.rgba(0.063, 0.725, 0.506, 0.16)
                        border.color: backend && backend.csvWriteBlocked
                                      ? Qt.rgba(0.937, 0.267, 0.267, 0.45)
                                      : Qt.rgba(0.063, 0.725, 0.506, 0.45)
                        width: csvStateText.implicitWidth + 16
                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Text {
                                text: backend && backend.csvWriteBlocked ? "⚠" : "✓"
                                color: backend && backend.csvWriteBlocked ? "#FCA5A5" : "#86EFAC"
                                font.pixelSize: 11
                            }
                            Text {
                                id: csvStateText
                                text: backend ? backend.csvWriteStatus : "CSV: Ready"
                                color: backend && backend.csvWriteBlocked ? "#FCA5A5" : "#86EFAC"
                                font.pixelSize: 11
                                font.bold: false
                            }
                        }
                    }
                    // Active status badge
                    Rectangle {
                        visible: measActive
                        height: 22; width: Math.max(activeLabel.implicitWidth || 60, 60) + 16
                        radius: 11
                        color: Qt.rgba(0.231, 0.510, 0.965, 0.18)
                        border.color: Qt.rgba(0.231, 0.510, 0.965, 0.45)
                        Text {
                            id: activeLabel
                            anchors.centerIn: parent
                            text: ["", "Heart Rate", "SpO₂", "Glucose", "Vitals"][measType] + "  " + measPct + "%"
                            color: "#93C5FD"; font.pixelSize: 11; font.bold: false
                        }
                    }
                }

                // Measurement buttons
                RowLayout {
                    spacing: 10

                    MeasButton {
                        label: "Heart Rate"; icon: "♥"
                        accent: "#EF4444"
                        isActive: measActive && (measType === 1 || measType === 4)
                        isDimmed: measActive && measType !== 1 && measType !== 4
                        progress: (measActive && (measType === 1 || measType === 4)) ? measPct / 100.0 : 0
                        onRequest: if (backend) backend.startVitals()
                    }

                    MeasButton {
                        label: "SpO₂"; icon: "○"
                        accent: "#3B82F6"
                        isActive: measActive && (measType === 2 || measType === 4)
                        isDimmed: measActive && measType !== 2 && measType !== 4
                        progress: (measActive && (measType === 2 || measType === 4)) ? measPct / 100.0 : 0
                        onRequest: if (backend) backend.startVitals()
                    }

                    MeasButton {
                        label: "Glucose"; icon: "◆"
                        accent: "#F59E0B"
                        isActive: measActive && measType === 3
                        isDimmed: measActive && measType !== 3
                        progress: (measActive && measType === 3) ? measPct / 100.0 : 0
                        onRequest: if (backend) backend.startGlucose()
                    }

                    Item { Layout.fillWidth: true }

                    // Stop button — only visible when active
                    Rectangle {
                        visible: measActive
                        height: 40; width: stopRow.implicitWidth + 28
                        radius: 8
                        color: stopMA.pressed ? Qt.rgba(0.937,0.267,0.267,0.40)
                             : stopMA.containsMouse ? Qt.rgba(0.937,0.267,0.267,0.28)
                             : Qt.rgba(0.937,0.267,0.267,0.16)
                        border.color: Qt.rgba(0.937,0.267,0.267,0.55)
                        Behavior on color { ColorAnimation { duration: 130 } }

                        Row { id: stopRow; anchors.centerIn: parent; spacing: 8
                            Text { text: "■"; color: "#FCA5A5"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: "Stop";  color: "#FCA5A5"; font.pixelSize: 14; font.bold: false; anchors.verticalCenter: parent.verticalCenter }
                        }
                        MouseArea { id: stopMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: if (backend) backend.stopMeasurement() }
                    }
                }

                // Progress track (thin line, visible only when active)
                Rectangle {
                    Layout.fillWidth: true; height: 3; radius: 2
                    color: Qt.rgba(1,1,1,0.06)
                    visible: measActive

                    Rectangle {
                        height: parent.height; radius: parent.radius
                        width: parent.width * (measPct / 100.0)
                        color: measType === 1 ? "#EF4444" : measType === 2 ? "#3B82F6" : "#F59E0B"
                        Behavior on width { NumberAnimation { duration: 200 } }
                    }
                }
            }
        }

        Rectangle {
            id: pmicCard
            Layout.fillWidth: true
            implicitHeight: pmicContent.implicitHeight + 32
            radius: 14
            color: Qt.rgba(1,1,1,0.04)
            border.color: Qt.rgba(0.961, 0.620, 0.043, 0.32)

            ColumnLayout {
                id: pmicContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 16
                spacing: 12

                RowLayout {
                    Text {
                        text: "PMIC Live Status"
                        color: "#FDE68A"
                        font.pixelSize: 15
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        height: 22
                        radius: 11
                        color: Qt.rgba(0.961, 0.620, 0.043, 0.18)
                        border.color: Qt.rgba(0.961, 0.620, 0.043, 0.48)
                        width: modeText.implicitWidth + 16
                        Text {
                            id: modeText
                            anchors.centerIn: parent
                            text: chargerSt === 1 ? "Mode: Charging" : chargerSt === 2 ? "Mode: Full" : "Mode: Discharging"
                            color: "#FDE68A"
                            font.pixelSize: 11
                        }
                    }
                }

                GridLayout {
                    columns: width > 920 ? 3 : (width > 620 ? 2 : 1)
                    columnSpacing: 10
                    rowSpacing: 10

                    MetricCard {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 220
                        label: "Battery"
                        valueText: battSoc > 0 ? battSoc + "" : "--"
                        unit: "%"
                        subText: battMv > 0 ? (battMv + " mV" + (isBatteryLow ? "  LOW" : "")) : ""
                        accent: isBatteryLow ? "#EF4444" : "#10B981"
                        iconText: "▮"
                        active: battSoc > 0 || battMv > 0
                    }

                    MetricCard {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 220
                        label: "Current"
                        valueText: battCurrentMa !== 0 ? battCurrentMa + "" : "0"
                        unit: "mA"
                        subText: avgCurrentMa !== 0 ? ("avg " + avgCurrentMa + " mA") : ""
                        accent: isCurrentHigh ? "#EF4444" : (battCurrentMa >= 0 ? "#F59E0B" : "#3B82F6")
                        iconText: battCurrentMa >= 0 ? "+" : "−"
                        active: true
                    }

                    MetricCard {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 220
                        label: "Battery Temp"
                        valueText: battTempC !== 0 ? battTempC + "" : "--"
                        unit: "°C"
                        subText: (battCycles > 0 ? ("cycles " + battCycles) : "") + (isBatteryHot ? "  HOT" : "")
                        accent: isBatteryHot ? "#EF4444" : "#06B6D4"
                        iconText: "◉"
                        active: battTempC !== 0 || battCycles > 0
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        radius: 10
                        height: 24
                        width: batteryAlertText.implicitWidth + 16
                        color: isBatteryLow ? Qt.rgba(0.937, 0.267, 0.267, 0.18) : Qt.rgba(0.063, 0.725, 0.506, 0.16)
                        border.color: isBatteryLow ? Qt.rgba(0.937, 0.267, 0.267, 0.50) : Qt.rgba(0.063, 0.725, 0.506, 0.40)
                        Text {
                            id: batteryAlertText
                            anchors.centerIn: parent
                            text: isBatteryLow ? "Battery Low" : "Battery OK"
                            color: isBatteryLow ? "#FCA5A5" : "#86EFAC"
                            font.pixelSize: 11
                        }
                    }

                    Rectangle {
                        radius: 10
                        height: 24
                        width: tempAlertText.implicitWidth + 16
                        color: isBatteryHot ? Qt.rgba(0.937, 0.267, 0.267, 0.18) : Qt.rgba(0.063, 0.725, 0.506, 0.16)
                        border.color: isBatteryHot ? Qt.rgba(0.937, 0.267, 0.267, 0.50) : Qt.rgba(0.063, 0.725, 0.506, 0.40)
                        Text {
                            id: tempAlertText
                            anchors.centerIn: parent
                            text: isBatteryHot ? "Temp High" : "Temp OK"
                            color: isBatteryHot ? "#FCA5A5" : "#86EFAC"
                            font.pixelSize: 11
                        }
                    }

                    Rectangle {
                        radius: 10
                        height: 24
                        width: currentAlertText.implicitWidth + 16
                        color: isCurrentHigh ? Qt.rgba(0.961, 0.620, 0.043, 0.18) : Qt.rgba(0.063, 0.725, 0.506, 0.16)
                        border.color: isCurrentHigh ? Qt.rgba(0.961, 0.620, 0.043, 0.50) : Qt.rgba(0.063, 0.725, 0.506, 0.40)
                        Text {
                            id: currentAlertText
                            anchors.centerIn: parent
                            text: isCurrentHigh ? "Current High" : "Current Normal"
                            color: isCurrentHigh ? "#FCD34D" : "#86EFAC"
                            font.pixelSize: 11
                        }
                    }

                }

                GridLayout {
                    columns: width > 920 ? 3 : 1
                    columnSpacing: 10
                    rowSpacing: 10

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 260
                        radius: 10
                        color: Qt.rgba(1,1,1,0.035)
                        border.color: Qt.rgba(1,1,1,0.10)
                        implicitHeight: 70

                        Column {
                            anchors.centerIn: parent
                            spacing: 4
                            Text { text: "Charger Setpoints"; color: "#94A3B8"; font.pixelSize: 11 }
                            Text {
                                text: (chgVmV > 0 ? chgVmV + " mV" : "--") + " / " +
                                      (chgImA > 0 ? chgImA + " mA" : "--")
                                color: "#F1F5F9"
                                font.pixelSize: 15
                                font.bold: true
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 260
                        radius: 10
                        color: Qt.rgba(1,1,1,0.035)
                        border.color: Qt.rgba(1,1,1,0.10)
                        implicitHeight: 70

                        Column {
                            anchors.centerIn: parent
                            spacing: 4
                            Text { text: "Rails"; color: "#94A3B8"; font.pixelSize: 11 }
                            Flow {
                                width: parent.width
                                spacing: 6
                                Repeater {
                                    model: [
                                        { label: "BK1", on: buck1En },
                                        { label: "BK2", on: buck2En },
                                        { label: "BK3", on: buck3En },
                                        { label: "BBOUT", on: bboutEn }
                                    ]
                                    delegate: Rectangle {
                                        radius: 7
                                        height: 20
                                        width: railLabel.implicitWidth + 12
                                        color: modelData.on ? Qt.rgba(0.063, 0.725, 0.506, 0.18) : Qt.rgba(0.937, 0.267, 0.267, 0.16)
                                        border.color: modelData.on ? Qt.rgba(0.063, 0.725, 0.506, 0.45) : Qt.rgba(0.937, 0.267, 0.267, 0.45)
                                        Text {
                                            id: railLabel
                                            anchors.centerIn: parent
                                            text: modelData.label + " " + (modelData.on ? "ON" : "OFF")
                                            color: modelData.on ? "#86EFAC" : "#FCA5A5"
                                            font.pixelSize: 10
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 260
                        radius: 10
                        color: Qt.rgba(1,1,1,0.035)
                        border.color: Qt.rgba(1,1,1,0.10)
                        implicitHeight: 70

                        Column {
                            anchors.centerIn: parent
                            spacing: 4
                            Text { text: "Rail Voltage"; color: "#94A3B8"; font.pixelSize: 11 }
                            Text {
                                text: "BK3 " + (buck3Mv > 0 ? buck3Mv + " mV" : "--") +
                                      "   BBOUT " + (bboutMv > 0 ? bboutMv + " mV" : "--")
                                color: "#F1F5F9"
                                font.pixelSize: 13
                            }
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------------------
        // Accelerometer section
        // ---------------------------------------------------------------
        Text {
            text: "Accelerometer"
            color: "#94A3B8"
            font.pixelSize: 13; font.bold: true
        }

        GridLayout {
            columns: width > 720 ? 3 : 1
            columnSpacing: 10
            rowSpacing: 10

            MetricCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 200
                label: "Accel X"
                valueText: accelX + ""
                unit: "mg"
                subText: ""
                accent: "#F59E0B"
                iconText: "→"
                active: accelActive
            }

            MetricCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 200
                label: "Accel Y"
                valueText: accelY + ""
                unit: "mg"
                subText: ""
                accent: "#A78BFA"
                iconText: "↑"
                active: accelActive
            }

            MetricCard {
                Layout.fillWidth: true
                Layout.preferredWidth: 200
                label: "Accel Z"
                valueText: accelZ + ""
                unit: "mg"
                subText: "‖ " + Math.round(Math.sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ)) + " mg"
                accent: "#34D399"
                iconText: "↓"
                active: accelActive
            }
        }

        Item { height: 24 }   // bottom padding inside ScrollView
    }
    } // ScrollView

    // -----------------------------------------------------------------------
    // Inline component: MetricCard
    // -----------------------------------------------------------------------
    component MetricCard: Rectangle {
        property string label: ""
        property string valueText: "--"
        property string unit: ""
        property string subText: ""
        property color  accent: "#3B82F6"
        property string iconText: "○"
        property bool   active: false

        height: 90; radius: 12
        color: Qt.rgba(1,1,1, 0.040)
        border.color: active ? Qt.rgba(accent.r, accent.g, accent.b, 0.30) : Qt.rgba(1,1,1, 0.07)
        Behavior on border.color { ColorAnimation { duration: 300 } }

        RowLayout {
            anchors { fill: parent; leftMargin: 14; rightMargin: 12 }
            spacing: 12

            // Icon circle
            Rectangle {
                width: 38; height: 38; radius: 19
                color: Qt.rgba(accent.r, accent.g, accent.b, active ? 0.20 : 0.10)
                Behavior on color { ColorAnimation { duration: 300 } }

                Text {
                    anchors.centerIn: parent
                    text: iconText
                    font.pixelSize: 16
                    color: active ? accent : "#475569"
                    Behavior on color { ColorAnimation { duration: 300 } }
                }
            }

            Column {
                spacing: 2; Layout.fillWidth: true
                Text { text: label; color: "#64748B"; font.pixelSize: 11 }
                Row {
                    spacing: 4
                    Text {
                        text: valueText
                        color: active ? "#F1F5F9" : "#334155"
                        font.pixelSize: 22; font.bold: true
                        Behavior on color { ColorAnimation { duration: 300 } }
                    }
                    Text {
                        text: unit
                        color: active ? "#94A3B8" : "#1E293B"
                        font.pixelSize: 13
                        anchors.baseline: parent.children[0].baseline
                        Behavior on color { ColorAnimation { duration: 300 } }
                    }
                }
                Text { text: subText; color: "#475569"; font.pixelSize: 11; visible: subText !== "" }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Inline component: MeasButton (120×64 with optional progress ring)
    // -----------------------------------------------------------------------
    component MeasButton: Rectangle {
        id: measBtn
        property string label: ""
        property string icon: "○"
        property color  accent: "#3B82F6"
        property bool   isActive: false
        property bool   isDimmed: false
        property real   progress: 0.0
        signal request()

        height: 64; width: 120; radius: 10
        color: isActive
               ? Qt.rgba(accent.r, accent.g, accent.b, 0.20)
               : (measBtnHover.hovered && !isDimmed)
                 ? Qt.rgba(accent.r, accent.g, accent.b, 0.12)
                 : Qt.rgba(1,1,1, 0.04)
        border.color: isActive ? Qt.rgba(accent.r, accent.g, accent.b, 0.55)
                                : Qt.rgba(accent.r, accent.g, accent.b, 0.20)
        opacity: isDimmed ? 0.35 : 1.0
        Behavior on color   { ColorAnimation { duration: 200 } }
        Behavior on opacity { NumberAnimation { duration: 200 } }

        // Progress arc overlay
        Canvas {
            id: progressCanvas
            anchors.fill: parent
            visible: measBtn.isActive
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var cx = width / 2, cy = height / 2
                var r  = Math.min(width, height) / 2 - 6
                // track
                ctx.beginPath()
                ctx.arc(cx, cy, r, 0, 2*Math.PI)
                ctx.strokeStyle = Qt.rgba(measBtn.accent.r, measBtn.accent.g, measBtn.accent.b, 0.12)
                ctx.lineWidth = 4
                ctx.stroke()
                // progress arc
                if (measBtn.progress > 0) {
                    ctx.beginPath()
                    ctx.arc(cx, cy, r, -Math.PI/2, -Math.PI/2 + measBtn.progress * 2*Math.PI)
                    ctx.strokeStyle = measBtn.accent
                    ctx.lineWidth = 4
                    ctx.lineCap = "round"
                    ctx.stroke()
                }
            }
            Connections {
                target: measBtn
                function onProgressChanged() { progressCanvas.requestPaint() }
                function onIsActiveChanged()  { progressCanvas.requestPaint() }
            }
        }

        Column {
            anchors.centerIn: parent; spacing: 4
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: measBtn.icon; font.pixelSize: 18; color: measBtn.isActive ? measBtn.accent : "#64748B" }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: measBtn.label; font.pixelSize: 12; font.bold: measBtn.isActive; color: measBtn.isActive ? "#F1F5F9" : "#64748B" }
        }

        HoverHandler { id: measBtnHover; enabled: !measBtn.isDimmed }
        TapHandler { enabled: !measBtn.isDimmed; onTapped: measBtn.request() }
    }

}
