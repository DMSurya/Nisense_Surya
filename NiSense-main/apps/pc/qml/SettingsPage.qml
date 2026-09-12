import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic

Page {
    id: settingsPage
    background: Rectangle { color: "#0A0E1A" }

    property int pmicBuck1En: 0
    property int pmicBuck2En: 0
    property int pmicBuck3En: 0
    property int pmicBboutEn: 0
    property int pmicBuck3Mv: 0
    property int pmicBboutMv: 0
    property int pmicChgVmV: 0
    property int pmicChgImA: 0

    // -----------------------------------------------------------------------
    // Disconnected gate overlay
    // -----------------------------------------------------------------------
    Rectangle {
        anchors.fill: parent; z: 10
        color: Qt.rgba(0.039, 0.055, 0.102, 0.90)
        visible: !(backend && backend.connected)
        radius: 0

        Column {
            anchors.centerIn: parent; spacing: 14
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "⚿"; font.pixelSize: 48; color: "#1E3A8A" }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Not connected"; color: "#F1F5F9"; font.pixelSize: 18; font.bold: true }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Connect to a device on the Scan page to adjust settings"; color: "#64748B"; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter }
        }
    }

    // -----------------------------------------------------------------------
    // Session info bar — always accessible, above the gate overlay
    // -----------------------------------------------------------------------
    Rectangle {
        id: sessionBar
        anchors { left: parent.left; right: parent.right; top: parent.top }
        anchors.margins: 16
        height: sessionBarCol.implicitHeight + 20
        radius: 10
        color:  Qt.rgba(1,1,1,0.038)
        border.color: Qt.rgba(0.067, 0.741, 0.506, 0.30)   // #11BD81 accent
        z: 11   // above the disconnected gate overlay

        // Left accent stripe
        Rectangle {
            x: 0; y: 8; width: 3; height: parent.height - 16; radius: 2
            color: "#10B981"
        }

        ColumnLayout {
            id: sessionBarCol
            anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
            anchors.leftMargin: 20; anchors.rightMargin: 16
            spacing: 8

        RowLayout {
            id: sessionBarRow
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "Patient Name"
                color: "#94A3B8"; font.pixelSize: 13
                Layout.preferredWidth: 110
            }

            TextField {
                id: patientNameField
                Layout.fillWidth: true
                text: backend ? backend.patientName : ""
                placeholderText: "Enter patient name (e.g. MPS)"
                color: "#F1F5F9"; placeholderTextColor: "#475569"
                background: Rectangle {
                    color: Qt.rgba(1,1,1,0.06); radius: 6
                    border.color: patientNameField.activeFocus
                        ? Qt.rgba(0.067, 0.741, 0.506, 0.70)
                        : Qt.rgba(1,1,1,0.12)
                }
                Connections {
                    target: backend
                    function onPatientNameChanged(n) { patientNameField.text = n }
                }
                onEditingFinished: if (backend) backend.setPatientName(text)
            }

            Text {
                text: "Desktop CSV only"
                color: "#475569"; font.pixelSize: 11
                Layout.preferredWidth: 160
            }

            Rectangle {
                height: 22
                radius: 11
                color: backend && backend.csvWriteBlocked
                       ? Qt.rgba(0.937, 0.267, 0.267, 0.16)
                       : Qt.rgba(0.063, 0.725, 0.506, 0.16)
                border.color: backend && backend.csvWriteBlocked
                              ? Qt.rgba(0.937, 0.267, 0.267, 0.45)
                              : Qt.rgba(0.063, 0.725, 0.506, 0.45)
                width: csvStatusText.implicitWidth + 16

                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Text {
                        text: backend && backend.csvWriteBlocked ? "⚠" : "✓"
                        color: backend && backend.csvWriteBlocked ? "#FCA5A5" : "#86EFAC"
                        font.pixelSize: 11
                    }
                    Text {
                        id: csvStatusText
                        text: backend ? backend.csvWriteStatus : "CSV: Ready"
                        color: backend && backend.csvWriteBlocked ? "#FCA5A5" : "#86EFAC"
                        font.pixelSize: 11
                    }
                }
            }

            Rectangle {
                width: 1
                height: 24
                color: Qt.rgba(1, 1, 1, 0.12)
            }

            Text {
                text: "Reconnect"
                color: "#94A3B8"
                font.pixelSize: 12
            }

            Switch {
                id: reconnectUnlimited
                checked: backend ? backend.autoReconnectUnlimited : true
                text: checked ? "Unlimited" : "Fixed"
                onToggled: if (backend) backend.setAutoReconnectUnlimited(checked)
            }

            SpinBox {
                id: reconnectAttemptsSpin
                from: 1; to: 1000; stepSize: 1
                enabled: !reconnectUnlimited.checked
                value: (backend && backend.autoReconnectMaxAttempts > 0)
                       ? backend.autoReconnectMaxAttempts : 5
                editable: true
                background: Rectangle { color: Qt.rgba(1,1,1,0.06); radius: 6; border.color: Qt.rgba(1,1,1,0.12) }
                contentItem: TextInput {
                    text: reconnectAttemptsSpin.textFromValue(reconnectAttemptsSpin.value, reconnectAttemptsSpin.locale)
                    color: "#F1F5F9"; font.pixelSize: 12
                    horizontalAlignment: Qt.AlignHCenter
                    readOnly: !reconnectAttemptsSpin.editable
                    validator: reconnectAttemptsSpin.validator
                    autoScroll: false
                    verticalAlignment: TextInput.AlignVCenter
                }
                onValueModified: if (backend) backend.setAutoReconnectMaxAttempts(value)
            }

            Text {
                text: "tries"
                color: "#64748B"
                font.pixelSize: 11
                visible: !reconnectUnlimited.checked
            }

            SpinBox {
                id: reconnectMaxDelaySpin
                from: 2; to: 120; stepSize: 1
                value: backend ? backend.autoReconnectMaxDelaySec : 20
                editable: true
                background: Rectangle { color: Qt.rgba(1,1,1,0.06); radius: 6; border.color: Qt.rgba(1,1,1,0.12) }
                contentItem: TextInput {
                    text: reconnectMaxDelaySpin.textFromValue(reconnectMaxDelaySpin.value, reconnectMaxDelaySpin.locale)
                    color: "#F1F5F9"; font.pixelSize: 12
                    horizontalAlignment: Qt.AlignHCenter
                    readOnly: !reconnectMaxDelaySpin.editable
                    validator: reconnectMaxDelaySpin.validator
                    autoScroll: false
                    verticalAlignment: TextInput.AlignVCenter
                }
                onValueModified: if (backend) backend.setAutoReconnectMaxDelaySec(value)
            }

            Text {
                text: "max s"
                color: "#64748B"
                font.pixelSize: 11
            }

            Connections {
                target: backend
                function onReconnectPolicyChanged() {
                    reconnectUnlimited.checked = backend.autoReconnectUnlimited
                    reconnectAttemptsSpin.value = (backend.autoReconnectMaxAttempts > 0)
                        ? backend.autoReconnectMaxAttempts : reconnectAttemptsSpin.value
                    reconnectMaxDelaySpin.value = backend.autoReconnectMaxDelaySec
                }
            }
        } // end sessionBarRow

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "Hardware ID"
                color: "#94A3B8"; font.pixelSize: 13
                Layout.preferredWidth: 110
            }

            Text {
                Layout.fillWidth: true
                text: backend && backend.hardwareDeviceId
                    ? backend.hardwareDeviceId
                    : "— connect to read from device"
                color: "#CBD5E1"; font.pixelSize: 13
                font.family: "Consolas"
                elide: Text.ElideMiddle
            }

            Text {
                text: "CSV Device_ID"
                color: "#475569"; font.pixelSize: 11
                Layout.preferredWidth: 160
            }
        }

        // Auto-measurement interval row
        RowLayout {
            id: measIntervalRow
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "Auto Measure"
                color: "#94A3B8"; font.pixelSize: 12
                Layout.preferredWidth: 110
            }

            Switch {
                id: measIntervalSwitch
                checked: backend ? backend.measIntervalEnabled : false
                text: checked ? "On" : "Off"
                onToggled: if (backend) backend.setMeasIntervalEnabled(checked)
            }

            SpinBox {
                id: measIntervalSpin
                from: 10; to: 3600; stepSize: 10
                value: backend ? backend.measIntervalSec : 60
                enabled: measIntervalSwitch.checked
                editable: true
                background: Rectangle { color: Qt.rgba(1,1,1,0.06); radius: 6; border.color: Qt.rgba(1,1,1,0.12) }
                contentItem: TextInput {
                    text: measIntervalSpin.textFromValue(measIntervalSpin.value, measIntervalSpin.locale)
                    color: "#F1F5F9"; font.pixelSize: 12
                    horizontalAlignment: Qt.AlignHCenter
                    readOnly: !measIntervalSpin.editable
                    validator: measIntervalSpin.validator
                    autoScroll: false
                    verticalAlignment: TextInput.AlignVCenter
                }
                onValueModified: if (backend) backend.setMeasIntervalSec(value)
            }

            Text { text: "s interval"; color: "#64748B"; font.pixelSize: 11 }

            Rectangle { width: 1; height: 20; color: Qt.rgba(1,1,1,0.12) }

            Text { text: "Type"; color: "#94A3B8"; font.pixelSize: 12 }

            ButtonGroup { id: measTypeGroup }

            Repeater {
                model: ["HR", "SpO2", "Both"]
                delegate: RadioButton {
                    required property int index
                    required property string modelData
                    text: modelData
                    checked: backend ? backend.measIntervalType === index : index === 0
                    enabled: measIntervalSwitch.checked
                    ButtonGroup.group: measTypeGroup
                    onToggled: if (checked && backend) backend.setMeasIntervalType(index)
                }
            }

            Item { Layout.fillWidth: true }

            Connections {
                target: backend
                function onMeasIntervalChanged() {
                    measIntervalSwitch.checked = backend.measIntervalEnabled
                    measIntervalSpin.value = backend.measIntervalSec
                }
            }
        } // end measIntervalRow

        } // end sessionBarCol
    }

    // -----------------------------------------------------------------------
    // Scrollable settings form (pushed down by sessionBar)
    // -----------------------------------------------------------------------
    ScrollView {
        anchors { left: parent.left; right: parent.right; top: sessionBar.bottom; bottom: parent.bottom }
        anchors.topMargin: 8
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: parent.parent.width
            anchors.left:   parent.left
            anchors.right:  parent.right
            anchors.top:    parent.top
            anchors.margins: 24
            spacing: 18

            // Page title
            Text { text: "Device Settings"; color: "#F1F5F9"; font.pixelSize: 22; font.bold: true }

            // -------------------------------------------------------
            // Device section
            // -------------------------------------------------------
            SettingsCard {
                title: "Device"; accentColor: "#3B82F6"; Layout.fillWidth: true

                ColumnLayout { spacing: 12
                    SettingsRow { label: "Device Name"
                        ctrl: TextField {
                            id: nameField
                            text: backend ? backend.deviceName : ""
                            placeholderText: "HCM"
                            color: "#F1F5F9"; placeholderTextColor: "#475569"
                            background: Rectangle { color: Qt.rgba(1,1,1,0.06); radius: 6; border.color: Qt.rgba(1,1,1,0.12) }
                            Connections { target: backend; function onDeviceNameChanged(n) { nameField.text = n } }
                            onEditingFinished: if (backend) backend.setDeviceName(text)
                        }
                    }

                    SettingsRow { label: "Brightness"
                        ctrl: RowLayout { spacing: 10
                            Slider { id: brightnessSlider; from: 0; to: 100; stepSize: 5; value: backend ? backend.brightness : 0
                                background: Rectangle { y: brightnessSlider.topPadding + brightnessSlider.availableHeight/2 - height/2; width: brightnessSlider.availableWidth; height: 4; radius: 2; color: Qt.rgba(1,1,1,0.10)
                                    Rectangle { width: brightnessSlider.visualPosition * parent.width; height: parent.height; radius: parent.radius; color: "#F59E0B" } }
                                handle: Rectangle { x: brightnessSlider.leftPadding + brightnessSlider.visualPosition * (brightnessSlider.availableWidth - width); y: brightnessSlider.topPadding + brightnessSlider.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: "#F59E0B"; border.color: Qt.rgba(1,1,1,0.3) }
                                Connections { target: backend; function onBrightnessChanged(v) { brightnessSlider.value = v } }
                                onPressedChanged: if (!pressed && backend) backend.setBrightness(value)
                            }
                            Text { text: brightnessSlider.value + "%"; color: "#94A3B8"; font.pixelSize: 12; width: 38 }
                        }
                    }

                    SettingsRow { label: "Volume"
                        ctrl: RowLayout { spacing: 10
                            Slider { id: volumeSlider; from: 0; to: 100; stepSize: 5; value: backend ? backend.volume : 0
                                background: Rectangle { y: volumeSlider.topPadding + volumeSlider.availableHeight/2 - height/2; width: volumeSlider.availableWidth; height: 4; radius: 2; color: Qt.rgba(1,1,1,0.10)
                                    Rectangle { width: volumeSlider.visualPosition * parent.width; height: parent.height; radius: parent.radius; color: "#8B5CF6" } }
                                handle: Rectangle { x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width); y: volumeSlider.topPadding + volumeSlider.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: "#8B5CF6"; border.color: Qt.rgba(1,1,1,0.3) }
                                Connections { target: backend; function onVolumeChanged(v) { volumeSlider.value = v } }
                                onPressedChanged: if (!pressed && backend) backend.setVolume(value)
                            }
                            Text { text: volumeSlider.value + "%"; color: "#94A3B8"; font.pixelSize: 12; width: 38 }
                        }
                    }

                    SettingsRow { label: "Battery Low (mV)"
                        ctrl: SpinBox { id: battLowSpin; from: 2500; to: 3500; stepSize: 50; value: backend ? backend.batteryLow : 3000
                            background: Rectangle { color: Qt.rgba(1,1,1,0.06); radius: 6; border.color: Qt.rgba(1,1,1,0.12) }
                            contentItem: TextInput { text: battLowSpin.textFromValue(battLowSpin.value, battLowSpin.locale); color: "#F1F5F9"; font.pixelSize: 13; horizontalAlignment: Qt.AlignHCenter; readOnly: !battLowSpin.editable; validator: battLowSpin.validator; autoScroll: false; verticalAlignment: TextInput.AlignVCenter }
                            Connections { target: backend; function onBatteryLowChanged(v) { battLowSpin.value = v } }
                            onValueModified: if (backend) backend.setBatteryLow(value)
                        }
                    }
                }
            }

            // -------------------------------------------------------
            // RTC section
            // -------------------------------------------------------
            SettingsCard {
                title: "Real-Time Clock"; accentColor: "#06B6D4"; Layout.fillWidth: true

                ColumnLayout { spacing: 12
                    SettingsRow { label: "RTC Trim (PPM)"
                        ctrl: RowLayout { spacing: 10
                            Slider { id: trimSlider; from: -200; to: 200; stepSize: 1; value: backend ? backend.rtcTrim : 0
                                background: Rectangle { y: trimSlider.topPadding + trimSlider.availableHeight/2 - height/2; width: trimSlider.availableWidth; height: 4; radius: 2; color: Qt.rgba(1,1,1,0.10)
                                    Rectangle { x: trimSlider.visualPosition < 0.5 ? trimSlider.visualPosition * parent.width : parent.width/2; width: Math.abs(trimSlider.visualPosition - 0.5) * parent.width; height: parent.height; radius: parent.radius; color: "#06B6D4" } }
                                handle: Rectangle { x: trimSlider.leftPadding + trimSlider.visualPosition * (trimSlider.availableWidth - width); y: trimSlider.topPadding + trimSlider.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: "#06B6D4"; border.color: Qt.rgba(1,1,1,0.3) }
                                Connections { target: backend; function onRtcTrimChanged(v) { trimSlider.value = v } }
                                onPressedChanged: if (!pressed && backend) backend.setRtcTrim(value)
                            }
                            Text { text: (trimSlider.value >= 0 ? "+" : "") + trimSlider.value + " ppm"; color: "#94A3B8"; font.pixelSize: 12; width: 72 }
                        }
                    }

                    ActionBtn { label: "Sync RTC to Host Time"; accent: "#06B6D4"; onRequest: if (backend) backend.syncRtcTime() }
                }
            }

            // -------------------------------------------------------
            // PPG section
            // -------------------------------------------------------
            SettingsCard {
                title: "PPG Streaming"; accentColor: "#10B981"; Layout.fillWidth: true

                ColumnLayout { spacing: 12
                    SettingsRow {
                        label: "PPG Source Preference"
                        ctrl: ComboBox {
                            id: ppgPrefCombo
                            model: [
                                { text: "Unset (auto unless both present)", value: 0 },
                                { text: "Auto", value: 1 },
                                { text: "Prefer MAX86141 (wearable)", value: 2 },
                                { text: "Prefer MAX3010x (pulse)", value: 3 }
                            ]
                            textRole: "text"
                            valueRole: "value"
                            currentIndex: {
                                if (!backend) return 0;
                                for (var i = 0; i < model.length; i++) {
                                    if (model[i].value === backend.ppgPreference) return i;
                                }
                                return 0;
                            }
                            onActivated: {
                                if (backend) backend.setPpgPreference(model[currentIndex].value)
                            }
                            Connections {
                                target: backend
                                function onPpgPreferenceChanged(v) {
                                    for (var i = 0; i < ppgPrefCombo.model.length; i++) {
                                        if (ppgPrefCombo.model[i].value === v) {
                                            ppgPrefCombo.currentIndex = i
                                            return
                                        }
                                    }
                                }
                            }
                            background: Rectangle { color: Qt.rgba(1,1,1,0.06); radius: 6; border.color: Qt.rgba(1,1,1,0.12) }
                            contentItem: Text { text: ppgPrefCombo.displayText; color: "#F1F5F9"; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
                            delegate: ItemDelegate {
                                width: ppgPrefCombo.width
                                contentItem: Text { text: modelData.text; color: "#F1F5F9"; elide: Text.ElideRight }
                                background: Rectangle { color: highlighted ? Qt.rgba(0.063,0.725,0.506,0.22) : Qt.rgba(1,1,1,0.00) }
                            }
                        }
                    }

                    SettingsRow {
                        label: "BLE Decimation  (1/" + decimateSlider.value + " = " + (33 / decimateSlider.value).toFixed(1) + " Hz)"
                        ctrl: Slider { id: decimateSlider; from: 1; to: 33; stepSize: 1; value: backend ? backend.ppgDecimate : 1
                            background: Rectangle { y: decimateSlider.topPadding + decimateSlider.availableHeight/2 - height/2; width: decimateSlider.availableWidth; height: 4; radius: 2; color: Qt.rgba(1,1,1,0.10)
                                Rectangle { width: decimateSlider.visualPosition * parent.width; height: parent.height; radius: parent.radius; color: "#10B981" } }
                            handle: Rectangle { x: decimateSlider.leftPadding + decimateSlider.visualPosition * (decimateSlider.availableWidth - width); y: decimateSlider.topPadding + decimateSlider.availableHeight/2 - height/2; width: 16; height: 16; radius: 8; color: "#10B981"; border.color: Qt.rgba(1,1,1,0.3) }
                            Connections { target: backend; function onPpgDecimateChanged(v) { decimateSlider.value = v } }
                            onPressedChanged: if (!pressed && backend) backend.setPpgDecimate(value)
                        }
                    }
                }
            }

            // -------------------------------------------------------
            // PMIC section
            // -------------------------------------------------------
            SettingsCard {
                title: "PMIC"; accentColor: "#F59E0B"; Layout.fillWidth: true

                ColumnLayout { spacing: 12
                    Connections {
                        target: backend
                        function onPmicExtUpdated(b3mv, bbmv, b1en, b2en, b3en, bben, chgVmV, chgImA,
                                                   battTc, cycles, rem, full, design, tte, ttf, avgMa) {
                            pmicBuck1En = b1en
                            pmicBuck2En = b2en
                            pmicBuck3En = b3en
                            pmicBboutEn = bben
                            pmicBuck3Mv = b3mv
                            pmicBboutMv = bbmv
                            pmicChgVmV = chgVmV
                            pmicChgImA = chgImA
                        }
                    }

                    SettingsRow {
                        label: "Target Rail"
                        ctrl: ComboBox {
                            id: pmicTargetCombo
                            model: [
                                { text: "BK1", value: 1 },
                                { text: "BK2", value: 2 },
                                { text: "BK3", value: 3 },
                                { text: "BBOUT", value: 4 }
                            ]
                            textRole: "text"
                            valueRole: "value"
                            currentIndex: 0
                            background: Rectangle { color: Qt.rgba(1,1,1,0.06); radius: 6; border.color: Qt.rgba(1,1,1,0.12) }
                            contentItem: Text { text: pmicTargetCombo.displayText; color: "#F1F5F9"; verticalAlignment: Text.AlignVCenter }
                            delegate: ItemDelegate {
                                width: pmicTargetCombo.width
                                contentItem: Text { text: modelData.text; color: "#F1F5F9"; elide: Text.ElideRight }
                                background: Rectangle { color: highlighted ? Qt.rgba(0.961,0.620,0.043,0.22) : Qt.rgba(1,1,1,0.00) }
                            }
                        }
                    }

                    RowLayout {
                        spacing: 8
                        ActionBtn {
                            label: "Enable"; accent: "#10B981"
                            onRequest: if (backend) backend.pmicEnable(pmicTargetCombo.model[pmicTargetCombo.currentIndex].value)
                        }
                        ActionBtn {
                            label: "Disable"; accent: "#EF4444"
                            onRequest: if (backend) backend.pmicDisable(pmicTargetCombo.model[pmicTargetCombo.currentIndex].value)
                        }
                    }

                    SettingsRow {
                        label: "Set Voltage (mV)"
                        ctrl: RowLayout {
                            spacing: 8
                            SpinBox {
                                id: pmicVoltageSpin
                                from: 600; to: 5500; stepSize: 50; value: 1800
                                background: Rectangle { color: Qt.rgba(1,1,1,0.06); radius: 6; border.color: Qt.rgba(1,1,1,0.12) }
                                contentItem: TextInput {
                                    text: pmicVoltageSpin.textFromValue(pmicVoltageSpin.value, pmicVoltageSpin.locale)
                                    color: "#F1F5F9"; font.pixelSize: 13
                                    horizontalAlignment: Qt.AlignHCenter
                                    readOnly: !pmicVoltageSpin.editable
                                    validator: pmicVoltageSpin.validator
                                    autoScroll: false
                                    verticalAlignment: TextInput.AlignVCenter
                                }
                            }
                            ActionBtn {
                                label: "Apply"; accent: "#F59E0B"
                                onRequest: if (backend) backend.pmicSetVoltage(
                                               pmicTargetCombo.model[pmicTargetCombo.currentIndex].value,
                                               pmicVoltageSpin.value)
                            }
                        }
                    }

                    Text {
                        text: "State: BK1 " + (pmicBuck1En ? "ON" : "OFF") +
                              " | BK2 " + (pmicBuck2En ? "ON" : "OFF") +
                              " | BK3 " + (pmicBuck3En ? "ON" : "OFF") +
                              " | BBOUT " + (pmicBboutEn ? "ON" : "OFF")
                        color: "#94A3B8"; font.pixelSize: 12
                    }
                    Text {
                        text: "Voltages: BK3 " + pmicBuck3Mv + " mV, BBOUT " + pmicBboutMv + " mV"
                        color: "#94A3B8"; font.pixelSize: 12
                    }
                    Text {
                        text: "Charger setpoints: " + pmicChgVmV + " mV / " + pmicChgImA + " mA"
                        color: "#94A3B8"; font.pixelSize: 12
                    }
                }
            }

            // -------------------------------------------------------
            // Wi-Fi section
            // -------------------------------------------------------
            SettingsCard {
                title: "Wi-Fi"; accentColor: "#3B82F6"; Layout.fillWidth: true

                ColumnLayout { spacing: 12
                    // Live status row — updated by firmware push notifications
                    SettingsRow { label: "Status"
                        ctrl: RowLayout {
                            spacing: 8
                            Rectangle {
                                width: 10; height: 10; radius: 5
                                color: {
                                    if (!backend) return "#6B7280"
                                    if (backend.wifiConnected === 2) return "#10B981"
                                    if (backend.wifiConnected === 1) return "#F59E0B"
                                    return "#6B7280"
                                }
                            }
                            Text {
                                color: {
                                    if (!backend) return "#94A3B8"
                                    if (backend.wifiConnected === 2) return "#10B981"
                                    if (backend.wifiConnected === 1) return "#F59E0B"
                                    return "#94A3B8"
                                }
                                font.pixelSize: 13
                                text: {
                                    if (!backend) return "\u2014"
                                    if (backend.wifiConnected === 2) {
                                        var s = "Connected"
                                        if (backend.wifiRssi !== 0) s += "  \u2022  " + backend.wifiRssi + " dBm"
                                        if (backend.wifiIp)         s += "  \u2022  " + backend.wifiIp
                                        return s
                                    }
                                    if (backend.wifiConnected === 1) return "Connecting\u2026"
                                    return "Disconnected"
                                }
                            }
                        }
                    }

                    SettingsRow { label: "SSID"
                        ctrl: TextField { id: ssidField; text: backend ? backend.wifiSsid : ""; placeholderText: "Network name"
                            color: "#F1F5F9"; placeholderTextColor: "#475569"
                            background: Rectangle { color: Qt.rgba(1,1,1,0.06); radius: 6; border.color: Qt.rgba(1,1,1,0.12) }
                            Connections { target: backend; function onWifiSsidChanged(v) { ssidField.text = v } }
                        }
                    }

                    SettingsRow { label: "Password"
                        ctrl: TextField { id: pskField; placeholderText: "Password"; echoMode: TextInput.Password
                            color: "#F1F5F9"; placeholderTextColor: "#475569"
                            background: Rectangle { color: Qt.rgba(1,1,1,0.06); radius: 6; border.color: Qt.rgba(1,1,1,0.12) }
                        }
                    }

                    RowLayout { spacing: 8
                        ActionBtn { label: "Save Credentials"; accent: "#3B82F6"; onRequest: if (backend) backend.setWifiCredentials(ssidField.text, pskField.text) }
                        ActionBtn { label: "Connect";          accent: "#10B981"; onRequest: if (backend) backend.wifiConnect() }
                        ActionBtn { label: "Disconnect";       accent: "#EF4444"; onRequest: if (backend) backend.wifiDisconnect() }
                    }
                }
            }

            Item { height: 20 }
        }
    }

    // -----------------------------------------------------------------------
    // Inline components
    // -----------------------------------------------------------------------
    component SettingsCard: Rectangle {
        id: sc
        property string title:       ""
        property color  accentColor: "#3B82F6"
        default property alias body: scBody.data

        height: scInner.implicitHeight + 36
        radius: 12
        color:  Qt.rgba(1,1,1,0.038)
        border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.22)

        // Left accent stripe
        Rectangle {
            x: 0; y: 12; width: 3; height: sc.height - 24; radius: 2
            color: sc.accentColor
        }

        ColumnLayout {
            id: scInner
            anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 20; rightMargin: 16; topMargin: 14 }
            spacing: 14

            Text { text: sc.title; color: "#F1F5F9"; font.pixelSize: 14; font.bold: true }

            Item {
                id: scBody
                Layout.fillWidth: true
                    implicitHeight: children.length > 0 && children[0] ? (children[0].implicitHeight || 48) : 0
            }
        }
    }

    component SettingsRow: RowLayout {
        property string label: ""
        property alias  ctrl:  ctrlSlot.children

        spacing: 16
        Text { text: label; color: "#94A3B8"; font.pixelSize: 13; Layout.preferredWidth: 180; Layout.alignment: Qt.AlignVCenter; wrapMode: Text.WordWrap }
            Item { id: ctrlSlot; Layout.fillWidth: true; implicitHeight: children.length > 0 && children[0] ? (children[0].implicitHeight || 32) : 0 }
    }

    component ActionBtn: Rectangle {
        id: ab
        property string label:  ""
        property color  accent: "#3B82F6"
        signal request()

        height: 34; width: abLbl.implicitWidth + 24; radius: 7
        color: abMA.pressed ? Qt.rgba(accent.r,accent.g,accent.b,0.35)
             : abMA.containsMouse ? Qt.rgba(accent.r,accent.g,accent.b,0.22)
             : Qt.rgba(accent.r,accent.g,accent.b,0.14)
        border.color: Qt.rgba(accent.r,accent.g,accent.b,0.50)
        Behavior on color { ColorAnimation { duration: 130 } }

        Text { id: abLbl; anchors.centerIn: parent; text: ab.label; color: Qt.lighter(ab.accent, 1.4); font.pixelSize: 13 }
        MouseArea { id: abMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: ab.request() }
    }
}



