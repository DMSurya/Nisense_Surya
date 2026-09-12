import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic
import Qt.labs.platform as Platform

Page {
    id: dfuPage
    background: Rectangle { color: "#0A0E1A" }

    // -----------------------------------------------------------------------
    // Local DFU state
    // -----------------------------------------------------------------------
    property string selectedFile: ""
    property int    bytesSent:    0
    property int    totalBytes:   0
    property string statusText:   "Select a signed firmware binary to begin."
    property string errorText:    ""
    property bool   dfuRunning:   backend ? backend.dfuActive : false
    // Pairing confirm dialog lives in main.qml (global, any page).

    // -----------------------------------------------------------------------
    // Wire backend DFU signals
    // -----------------------------------------------------------------------
    Connections {
        target: backend
        function onDfuProgress(sent, total) {
            dfuPage.bytesSent = sent
            dfuPage.totalBytes = total
        }
        function onDfuStatus(msg) {
            dfuPage.statusText = msg
        }
        function onDfuError(msg) {
            dfuPage.errorText = msg
        }
        function onDfuActiveChanged(active) {
            dfuPage.dfuRunning = active
        }
    }

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
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Connect to a device on the Scan page before starting a firmware update."; color: "#64748B"; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter }
        }
    }

    // -----------------------------------------------------------------------
    // Native file dialog
    // -----------------------------------------------------------------------
    Platform.FileDialog {
        id: fileDialog
        title: "Select Signed Firmware Binary"
        nameFilters: ["Firmware images (*.bin *.hex)", "All files (*)"]
        onAccepted: {
            // Convert QUrl to local path (strip "file:///", handle Windows drive)
            var raw = fileDialog.file.toString()
            // file:///C:/path/... → C:/path/...
            var path = raw.replace(/^file:\/\/\//, "").replace(/^file:\/\//, "")
            // On Windows, forward slashes are fine for Python pathlib
            dfuPage.selectedFile = path
            dfuPage.errorText    = ""
            dfuPage.statusText   = "Ready — press Flash to start upload."
        }
    }

    // -----------------------------------------------------------------------
    // Main content
    // -----------------------------------------------------------------------
    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 0

            // ---------------------------------------------------------------
            // Page title
            // ---------------------------------------------------------------
            Item { Layout.fillWidth: true; height: 24 }

            Text {
                Layout.leftMargin: 24
                text: "Firmware Update (BLE DFU)"
                color: "#F1F5F9"
                font.pixelSize: 20
                font.weight: Font.Medium
            }
            Text {
                Layout.leftMargin: 24
                Layout.topMargin: 4
                text: "Upload a signed firmware image via MCUmgr SMP over BLE"
                color: "#64748B"
                font.pixelSize: 13
            }

            Item { Layout.fillWidth: true; height: 24 }

            // ---------------------------------------------------------------
            // Card: image selection
            // ---------------------------------------------------------------
            Rectangle {
                Layout.leftMargin: 16; Layout.rightMargin: 16
                Layout.fillWidth: true
                height: selCol.implicitHeight + 32
                radius: 10
                color: Qt.rgba(1, 1, 1, 0.038)
                border.color: Qt.rgba(1, 1, 1, 0.08)

                ColumnLayout {
                    id: selCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
                    spacing: 12

                    Text {
                        text: "Firmware Image"
                        color: "#94A3B8"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        font.letterSpacing: 1.2
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.fillWidth: true
                            height: 38
                            radius: 6
                            color: Qt.rgba(1, 1, 1, 0.05)
                            border.color: Qt.rgba(1, 1, 1, 0.12)

                            Text {
                                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 10; rightMargin: 10 }
                                text: dfuPage.selectedFile !== "" ? dfuPage.selectedFile : "No file selected"
                                color: dfuPage.selectedFile !== "" ? "#F1F5F9" : "#475569"
                                font.pixelSize: 12
                                elide: Text.ElideLeft
                            }
                        }

                        Button {
                            text: "Browse…"
                            enabled: !dfuPage.dfuRunning
                            onClicked: fileDialog.open()

                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? "#F1F5F9" : "#475569"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                radius: 6
                                color: parent.pressed ? Qt.rgba(0.118, 0.227, 0.541, 0.9)
                                     : parent.hovered ? Qt.rgba(0.118, 0.227, 0.541, 0.7)
                                     : Qt.rgba(0.118, 0.227, 0.541, 0.5)
                                border.color: Qt.rgba(0.576, 0.773, 0.992, 0.3)
                            }
                            implicitWidth: 90
                            implicitHeight: 38
                        }
                    }

                    // Build-output hint
                    Text {
                        Layout.fillWidth: true
                        text: "Tip: use build_sdk_v330/NiSense/zephyr/zephyr.signed.bin"
                        color: "#475569"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Item { Layout.fillWidth: true; height: 16 }

            // ---------------------------------------------------------------
            // Card: progress + status
            // ---------------------------------------------------------------
            Rectangle {
                Layout.leftMargin: 16; Layout.rightMargin: 16
                Layout.fillWidth: true
                height: progressCol.implicitHeight + 32
                radius: 10
                color: Qt.rgba(1, 1, 1, 0.038)
                border.color: Qt.rgba(1, 1, 1, 0.08)

                ColumnLayout {
                    id: progressCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 16 }
                    spacing: 12

                    Text {
                        text: "Status"
                        color: "#94A3B8"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        font.letterSpacing: 1.2
                    }

                    // Progress bar
                    Rectangle {
                        Layout.fillWidth: true
                        height: 8
                        radius: 4
                        color: Qt.rgba(1, 1, 1, 0.08)

                        Rectangle {
                            width: dfuPage.totalBytes > 0
                                   ? Math.max(8, parent.width * dfuPage.bytesSent / dfuPage.totalBytes)
                                   : 0
                            height: parent.height
                            radius: parent.radius
                            color: dfuPage.dfuRunning ? "#3B82F6" : "#11BD81"

                            Behavior on width { NumberAnimation { duration: 120 } }
                        }
                    }

                    // Percentage + byte count
                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: dfuPage.totalBytes > 0
                                  ? Math.floor(dfuPage.bytesSent * 100 / dfuPage.totalBytes) + "%"
                                  : "—"
                            color: "#F1F5F9"
                            font.pixelSize: 24
                            font.weight: Font.Bold
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            visible: dfuPage.totalBytes > 0
                            text: dfuPage.bytesSent + " / " + dfuPage.totalBytes + " bytes"
                            color: "#64748B"
                            font.pixelSize: 12
                        }
                    }

                    // Status string
                    Text {
                        Layout.fillWidth: true
                        text: dfuPage.statusText
                        color: "#CBD5E1"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }

                    // Error display (only shown when non-empty)
                    Rectangle {
                        visible: dfuPage.errorText !== ""
                        Layout.fillWidth: true
                        height: errText.implicitHeight + 16
                        radius: 6
                        color: Qt.rgba(0.749, 0.216, 0.216, 0.15)
                        border.color: Qt.rgba(0.749, 0.216, 0.216, 0.4)

                        Text {
                            id: errText
                            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
                            text: "⚠  " + dfuPage.errorText
                            color: "#FCA5A5"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true; height: 20 }

            // ---------------------------------------------------------------
            // Action buttons
            // ---------------------------------------------------------------
            RowLayout {
                Layout.leftMargin: 16; Layout.rightMargin: 16
                Layout.fillWidth: true
                spacing: 12

                // Pair button — manually trigger authenticated pairing
                Button {
                    text: "🔗  Pair"
                    enabled: !dfuPage.dfuRunning && (backend && backend.connected && !backend.isConnecting)
                    implicitWidth: 120
                    implicitHeight: 44
                    onClicked: backend.pairDevice()
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#BFDBFE" : "#475569"
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: parent.pressed ? Qt.rgba(0.145, 0.388, 0.922, 0.35)
                             : parent.hovered ? Qt.rgba(0.145, 0.388, 0.922, 0.25)
                                              : Qt.rgba(0.145, 0.388, 0.922, 0.15)
                        border.color: parent.enabled ? Qt.rgba(0.145, 0.388, 0.922, 0.4) : Qt.rgba(1,1,1,0.08)
                    }
                }

                // Clear PC bond — WinRT unpair only (fast; use when pairing is wedged)
                Button {
                    text: "Clear PC Bond"
                    enabled: !dfuPage.dfuRunning && backend
                    implicitWidth: 130
                    implicitHeight: 44
                    ToolTip.text: "Remove Windows bond only (~2 s). Does not clear bonds on the watch."
                    ToolTip.visible: hovered
                    onClicked: backend.forgetPairing()
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#FCD34D" : "#475569"
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: parent.pressed ? Qt.rgba(0.918, 0.702, 0.031, 0.30)
                             : parent.hovered ? Qt.rgba(0.918, 0.702, 0.031, 0.20)
                                              : Qt.rgba(0.918, 0.702, 0.031, 0.12)
                        border.color: parent.enabled ? Qt.rgba(0.918, 0.702, 0.031, 0.4) : Qt.rgba(1,1,1,0.08)
                    }
                }

                // Manual recovery — unpair + bounce Bluetooth radio (~45 s)
                Button {
                    text: "Reset BT"
                    enabled: !dfuPage.dfuRunning && backend
                    implicitWidth: 100
                    implicitHeight: 44
                    ToolTip.text: "Clear PC bond and bounce Bluetooth radio (~45 s). Use if Clear PC Bond is not enough."
                    ToolTip.visible: hovered
                    onClicked: backend.recoverWindowsPairing(true)
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#FCA5A5" : "#475569"
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: parent.pressed ? Qt.rgba(0.937, 0.267, 0.267, 0.35)
                             : parent.hovered ? Qt.rgba(0.937, 0.267, 0.267, 0.22)
                                              : Qt.rgba(0.937, 0.267, 0.267, 0.12)
                        border.color: parent.enabled ? Qt.rgba(0.937, 0.267, 0.267, 0.4) : Qt.rgba(1,1,1,0.08)
                    }
                }

                // Flash button
                Button {
                    id: flashBtn
                    text: dfuPage.dfuRunning ? "Uploading…" : "⬆  Flash Firmware"
                    enabled: !dfuPage.dfuRunning && dfuPage.selectedFile !== "" && (backend && backend.connected)
                    Layout.fillWidth: true
                    implicitHeight: 44

                    onClicked: {
                        dfuPage.errorText  = ""
                        dfuPage.statusText = "Starting upload…"
                        backend.startDfuUpload(dfuPage.selectedFile)
                    }

                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#F1F5F9" : "#475569"
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: parent.pressed ? "#1D4ED8"
                             : parent.hovered ? "#2563EB"
                             : parent.enabled ? "#3B82F6"
                                              : Qt.rgba(1,1,1,0.06)
                        border.color: parent.enabled ? "transparent" : Qt.rgba(1,1,1,0.08)
                    }
                }

                // Cancel button — only visible while running
                Button {
                    text: "Cancel"
                    visible: dfuPage.dfuRunning
                    implicitWidth: 100
                    implicitHeight: 44

                    onClicked: backend.cancelDfu()

                    contentItem: Text {
                        text: parent.text
                        color: "#FCA5A5"
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: 8
                        color: parent.pressed ? Qt.rgba(0.749, 0.216, 0.216, 0.35)
                             : parent.hovered ? Qt.rgba(0.749, 0.216, 0.216, 0.25)
                                              : Qt.rgba(0.749, 0.216, 0.216, 0.15)
                        border.color: Qt.rgba(0.749, 0.216, 0.216, 0.4)
                    }
                }
            }

            // ---------------------------------------------------------------
            // Info box — update procedure steps
            // ---------------------------------------------------------------
            Item { Layout.fillWidth: true; height: 24 }

            Rectangle {
                Layout.leftMargin: 16; Layout.rightMargin: 16
                Layout.fillWidth: true
                height: infoCol.implicitHeight + 24
                radius: 10
                color: Qt.rgba(0.118, 0.227, 0.541, 0.10)
                border.color: Qt.rgba(0.576, 0.773, 0.992, 0.15)

                ColumnLayout {
                    id: infoCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                    spacing: 6

                    Text {
                        text: "Update procedure"
                        color: "#93C5FD"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }
                    Repeater {
                        model: [
                            "1. Build firmware and locate zephyr.signed.bin in the build output.",
                            "2. Connect to the device via the Scan page.",
                            "3. Browse to the signed binary and press Flash Firmware.",
                            "4. Wait for the upload to complete (≈ 2–4 min over BLE).",
                            "5. The device reboots automatically.  MCUboot verifies the RSA-2048 signature and swaps the image.",
                            "6. If the new firmware boots successfully it becomes permanent; otherwise MCUboot reverts to the previous image."
                        ]
                        delegate: Text {
                            Layout.fillWidth: true
                            text: modelData
                            color: "#64748B"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true; height: 24 }
        }
    }
}
