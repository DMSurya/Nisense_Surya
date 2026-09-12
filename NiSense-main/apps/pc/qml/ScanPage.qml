import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: scanPage
    background: Rectangle { color: "#0A0E1A" }

    // -----------------------------------------------------------------------
    // Merge known devices + scan results into one display model
    // -----------------------------------------------------------------------
    property var _mergedList: []

    function _rebuildList() {
        var known = (backend && backend.knownDevices) ? backend.knownDevices : []
        var scanned = (backend && backend.scanResults) ? backend.scanResults : []
        var out = []

        // Start with known devices; mark them trusted
        for (var k = 0; k < known.length; k++) {
            var entry = { name: known[k].name || "HCM",
                          address: known[k].address,
                          rssi: -99,
                          trusted: true,
                          last_seen: known[k].last_seen || 0 }
            // Overlay rssi if found in scan results
            for (var s = 0; s < scanned.length; s++) {
                if (scanned[s].address.toLowerCase() === known[k].address.toLowerCase()) {
                    entry.rssi = scanned[s].rssi
                    break
                }
            }
            out.push(entry)
        }
        // Add scan results not already in known list
        for (var s2 = 0; s2 < scanned.length; s2++) {
            var alreadyIn = false
            for (var k2 = 0; k2 < known.length; k2++) {
                if (scanned[s2].address.toLowerCase() === known[k2].address.toLowerCase()) {
                    alreadyIn = true; break
                }
            }
            if (!alreadyIn)
                out.push({ name: scanned[s2].name || "HCM",
                            address: scanned[s2].address,
                            rssi: scanned[s2].rssi,
                            trusted: false,
                            last_seen: 0 })
        }
        _mergedList = out
        deviceListModel.clear()
        for (var i = 0; i < out.length; i++) deviceListModel.append(out[i])
    }

    ListModel { id: deviceListModel }

    Connections {
        target: backend
        function onKnownDevicesChanged() { scanPage._rebuildList() }
        function onScanResultsChanged()  { scanPage._rebuildList() }
    }

    Component.onCompleted: _rebuildList()

    // -----------------------------------------------------------------------
    // Layout
    // -----------------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        // Title row + action buttons
        RowLayout {
            spacing: 12

            Column {
                spacing: 2
                Text {
                    text: "Devices"
                    color: "#F1F5F9"
                    font.pixelSize: 22; font.weight: Font.Bold
                }
                Text {
                    text: backend && backend.connected
                          ? "Connected — " + (backend.deviceName || "HCM")
                          : "Not connected"
                    color: backend && backend.connected ? "#6EE7B7" : "#64748B"
                    font.pixelSize: 13
                }
            }

            Item { Layout.fillWidth: true }

            // Auto-Connect pill toggle
            Rectangle {
                id: autoConnToggle
                property bool isOn: (backend && backend.knownDevices && backend.knownDevices.length > 0)

                height: 36
                width: autoConnRow.implicitWidth + 24
                radius: 18
                color: isOn ? Qt.rgba(0.063, 0.725, 0.506, 0.20)
                             : Qt.rgba(1, 1, 1, 0.06)
                border.color: isOn ? Qt.rgba(0.063, 0.725, 0.506, 0.50)
                                   : Qt.rgba(1, 1, 1, 0.12)
                Behavior on color        { ColorAnimation { duration: 200 } }
                Behavior on border.color { ColorAnimation { duration: 200 } }

                Row {
                    id: autoConnRow
                    anchors.centerIn: parent
                    spacing: 7

                    Text {
                        text: autoConnToggle.isOn ? "🔗" : "⚡"
                        font.pixelSize: 14
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: autoConnToggle.isOn ? "Auto-Connect  ON" : "Auto-Connect  OFF"
                        color: autoConnToggle.isOn ? "#6EE7B7" : "#94A3B8"
                        font.pixelSize: 13; font.weight: Font.Normal
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (backend && !backend.connected && !backend.isConnecting) {
                            backend.autoConnectKnown()
                        }
                    }
                }

                ToolTip.visible: autoConnHover.hovered
                ToolTip.text: autoConnToggle.isOn
                              ? "Known device saved — click to auto-connect"
                              : "Connect to last known device"
                ToolTip.delay: 500
                HoverHandler { id: autoConnHover }
            }

            // Scan button — wired to backend.isScanning
            Rectangle {
                id: scanBtn
                property bool _busy: backend ? backend.isScanning : false
                property bool _done: false

                height: 36
                width:  scanBtnRow.implicitWidth + 24
                radius: 8
                color: _busy ? Qt.rgba(0.231, 0.510, 0.965, 0.12)
                             : (scanBtnHover.hovered
                                ? Qt.rgba(0.231, 0.510, 0.965, 0.22)
                                : Qt.rgba(0.231, 0.510, 0.965, 0.15))
                border.color: _busy ? Qt.rgba(0.231, 0.510, 0.965, 0.35)
                                    : Qt.rgba(0.231, 0.510, 0.965, 0.55)
                Behavior on color        { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }
                opacity: _busy ? 0.7 : 1.0
                Behavior on opacity { NumberAnimation { duration: 150 } }

                Row {
                    id: scanBtnRow
                    anchors.centerIn: parent
                    spacing: 8

                    BusyIndicator {
                        visible: scanBtn._busy
                        running: scanBtn._busy
                        width: 16; height: 16
                        anchors.verticalCenter: parent.verticalCenter
                        palette.dark: "#3B82F6"
                        palette.mid:  Qt.rgba(0.231, 0.510, 0.965, 0.25)
                    }
                    Text {
                        text: scanBtn._busy ? "Scanning…" : "⊕  Scan"
                        color: "#93C5FD"
                        font.pixelSize: 14; font.weight: Font.Normal
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                MouseArea {
                    id: scanBtnMA
                    anchors.fill: parent
                    enabled: !scanBtn._busy
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (backend) backend.scan()
                }
                HoverHandler { id: scanBtnHover; enabled: !scanBtn._busy }

                // Track scan completion for brief success flash
                Connections {
                    target: backend
                    function onScanComplete(count) {
                        scanBtn._done = true
                        scanDoneTimer.restart()
                    }
                }
                Timer {
                    id: scanDoneTimer; interval: 1200
                    onTriggered: scanBtn._done = false
                }
            }
        }

        // Scan progress bar — visible while scanning
        Rectangle {
            Layout.fillWidth: true
            height: 3; radius: 2
            color: Qt.rgba(1,1,1,0.06)
            visible: backend && backend.isScanning

            Rectangle {
                id: progressBar
                height: parent.height; radius: parent.radius
                color: "#3B82F6"
                width: parent.width * 0.35
                SequentialAnimation on x {
                    running: backend && backend.isScanning
                    loops: Animation.Infinite
                    NumberAnimation { from: -progressBar.width; to: parent.parent.width; duration: 1400; easing.type: Easing.InOutQuad }
                }
            }
        }

        // ---------------------------------------------------------------
        // Unified device list
        // ---------------------------------------------------------------
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Empty state
            Column {
                anchors.centerIn: parent
                spacing: 12
                visible: deviceListModel.count === 0

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "⊕"
                    font.pixelSize: 48; color: "#1E3A8A"
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "No devices found"
                    color: "#475569"; font.pixelSize: 16; font.weight: Font.Normal
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Press Scan to search for HCM devices"
                    color: "#334155"; font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            ListView {
                id: deviceList
                anchors.fill: parent
                clip: true
                spacing: 8
                model: deviceListModel

                delegate: Rectangle {
                    id: devCard
                    width: deviceList.width
                    height: 76
                    radius: 12
                    color: devCardHover.hovered
                           ? Qt.rgba(1,1,1,0.065)
                           : Qt.rgba(1,1,1,0.040)
                    border.color: model.trusted
                                  ? Qt.rgba(0.063, 0.725, 0.506, 0.28)
                                  : Qt.rgba(1,1,1,0.07)
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 150 } }

                    HoverHandler { id: devCardHover }

                    RowLayout {
                        anchors { fill: parent; leftMargin: 16; rightMargin: 12 }
                        spacing: 14

                        // Signal strength bars
                        Column {
                            spacing: 2
                            Layout.alignment: Qt.AlignVCenter

                            // 4 bars from bottom up
                            Repeater {
                                model: 4
                                Rectangle {
                                    property int idx: 3 - index   // bars from 0 (weakest) to 3 (strongest)
                                    property int barCount: {
                                        var r = model.rssi || -99
                                        if (r >= -55) return 4
                                        if (r >= -67) return 3
                                        if (r >= -80) return 2
                                        if (r >= -92) return 1
                                        return 0
                                    }
                                    width: 5
                                    height: (idx + 1) * 5
                                    radius: 2
                                    color: idx < barCount ? "#3B82F6" : Qt.rgba(1,1,1,0.12)
                                }
                            }
                        }

                        // Name + address + badges
                        Column {
                            spacing: 4
                            Layout.alignment: Qt.AlignVCenter
                            Layout.fillWidth: true

                            Row {
                                spacing: 8
                                Text {
                                    text: model.name || "HCM"
                                    color: "#F1F5F9"
                                    font.pixelSize: 15; font.weight: Font.Bold
                                }
                                // Trusted badge
                                Rectangle {
                                    visible: model.trusted
                                    height: 18; width: trustedLabel.implicitWidth + 10
                                    radius: 9
                                    color: Qt.rgba(0.063, 0.725, 0.506, 0.16)
                                    border.color: Qt.rgba(0.063, 0.725, 0.506, 0.40)
                                    Text {
                                        id: trustedLabel
                                        anchors.centerIn: parent
                                        text: "🔒 Trusted"
                                        color: "#6EE7B7"
                                        font.pixelSize: 10; font.weight: Font.Normal
                                    }
                                }
                                // RSSI badge (only when scanned)
                                Rectangle {
                                    visible: model.rssi !== -99 && model.rssi !== 0
                                    height: 18; width: rssiLabel.implicitWidth + 10
                                    radius: 9
                                    color: Qt.rgba(1,1,1,0.06)
                                    border.color: Qt.rgba(1,1,1,0.12)
                                    Text {
                                        id: rssiLabel
                                        anchors.centerIn: parent
                                        text: (model.rssi || -99) + " dBm"
                                        color: "#94A3B8"; font.pixelSize: 10
                                    }
                                }
                            }

                            Text {
                                text: model.address || ""
                                color: "#64748B"; font.pixelSize: 12
                                font.family: "Consolas, monospace"
                            }
                        }

                        // Action buttons
                        Row {
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter

                            // Per-device connect/connecting/disconnect button
                            Rectangle {
                                id: connBtn

                                // State helpers
                                property bool isThisConnected: backend
                                    ? (backend.connected && backend.connectedAddress.toLowerCase() === model.address.toLowerCase())
                                    : false
                                property bool isThisConnecting: backend
                                    ? (backend.isConnecting && backend.connectingAddress.toLowerCase() === model.address.toLowerCase())
                                    : false
                                // Disable Connect when already connected to a different device or connecting to another
                                property bool canConnect: backend
                                    ? (!backend.connected && !backend.isConnecting)
                                    : false

                                height: 32
                                width: connBtnRow.implicitWidth + 24
                                radius: 7
                                opacity: (!isThisConnected && !isThisConnecting && !canConnect) ? 0.4 : 1.0
                                Behavior on opacity { NumberAnimation { duration: 150 } }

                                // Color: red when connected, amber when connecting, cyan otherwise
                                color: isThisConnected
                                    ? (connBtnMA.pressed ? Qt.rgba(0.937, 0.267, 0.267, 0.35)
                                       : connBtnMA.containsMouse ? Qt.rgba(0.937, 0.267, 0.267, 0.26)
                                       : Qt.rgba(0.937, 0.267, 0.267, 0.16))
                                    : isThisConnecting
                                    ? Qt.rgba(0.961, 0.620, 0.043, 0.14)
                                    : (connBtnMA.pressed ? Qt.rgba(0.024, 0.714, 0.831, 0.30)
                                       : connBtnMA.containsMouse ? Qt.rgba(0.024, 0.714, 0.831, 0.20)
                                       : Qt.rgba(0.024, 0.714, 0.831, 0.14))
                                border.color: isThisConnected
                                    ? Qt.rgba(0.937, 0.267, 0.267, 0.60)
                                    : isThisConnecting
                                    ? Qt.rgba(0.961, 0.620, 0.043, 0.55)
                                    : Qt.rgba(0.024, 0.714, 0.831, 0.55)
                                Behavior on color        { ColorAnimation { duration: 150 } }
                                Behavior on border.color { ColorAnimation { duration: 150 } }

                                Row {
                                    id: connBtnRow
                                    anchors.centerIn: parent
                                    spacing: 6

                                    BusyIndicator {
                                        visible: connBtn.isThisConnecting
                                        running: connBtn.isThisConnecting
                                        width: 14; height: 14
                                        anchors.verticalCenter: parent.verticalCenter
                                        palette.dark: "#FBBF24"
                                        palette.mid:  Qt.rgba(0.961, 0.620, 0.043, 0.25)
                                    }
                                    Text {
                                        id: connBtnLabel
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: connBtn.isThisConnected
                                              ? "Disconnect"
                                              : connBtn.isThisConnecting
                                              ? "Connecting…"
                                              : "Connect"
                                        color: connBtn.isThisConnected
                                               ? "#FCA5A5"
                                               : connBtn.isThisConnecting
                                               ? "#FCD34D"
                                               : "#67E8F9"
                                        font.pixelSize: 13; font.weight: Font.Normal
                                    }
                                }

                                MouseArea {
                                    id: connBtnMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    enabled: connBtn.isThisConnected || connBtn.canConnect
                                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: {
                                        if (!backend) return
                                        if (connBtn.isThisConnected) {
                                            backend.disconnect()
                                        } else if (connBtn.canConnect) {
                                            backend.connectToAddress(model.address)
                                        }
                                    }
                                }
                            }

                            // Unpair All — firmware bonds + Windows bond + remove from list
                            Rectangle {
                                visible: model.trusted
                                height: 32; width: unpairLabel.implicitWidth + 20
                                radius: 7
                                color: unpairMA.pressed ? Qt.rgba(0.545, 0.110, 0.604, 0.30)
                                     : unpairMA.containsMouse ? Qt.rgba(0.545, 0.110, 0.604, 0.18)
                                     : Qt.rgba(0.545, 0.110, 0.604, 0.10)
                                border.color: Qt.rgba(0.545, 0.110, 0.604, 0.40)
                                Behavior on color { ColorAnimation { duration: 120 } }

                                Text {
                                    id: unpairLabel
                                    anchors.centerIn: parent
                                    text: "Unpair All"
                                    color: "#C084FC"; font.pixelSize: 13
                                }
                                MouseArea {
                                    id: unpairMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    ToolTip.text: "Clear bonds on watch and PC, then remove from list"
                                    ToolTip.visible: containsMouse
                                    onClicked: if (backend) backend.unpairDevice(model.address)
                                }
                            }

                            // Remove — list only (no OS / firmware unpair)
                            Rectangle {
                                visible: model.trusted
                                height: 32; width: forgetLabel.implicitWidth + 20
                                radius: 7
                                color: forgetMA.pressed ? Qt.rgba(0.937, 0.267, 0.267, 0.28)
                                     : forgetMA.containsMouse ? Qt.rgba(0.937, 0.267, 0.267, 0.15)
                                     : Qt.rgba(0.937, 0.267, 0.267, 0.08)
                                border.color: Qt.rgba(0.937, 0.267, 0.267, 0.35)
                                Behavior on color { ColorAnimation { duration: 120 } }

                                Text {
                                    id: forgetLabel
                                    anchors.centerIn: parent
                                    text: "Remove"
                                    color: "#FCA5A5"; font.pixelSize: 13
                                }
                                MouseArea {
                                    id: forgetMA
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    ToolTip.text: "Stop tracking this device (does not clear PC or watch bonds)"
                                    ToolTip.visible: containsMouse
                                    onClicked: if (backend) backend.forgetDevice(model.address)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

