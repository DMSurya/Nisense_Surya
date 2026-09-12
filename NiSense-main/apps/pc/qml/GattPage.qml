import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic

Page {
    id: gattPage
    background: Rectangle { color: "#0A0E1A" }

    // -----------------------------------------------------------------------
    // State — parsed from backend.gattTableJson
    // -----------------------------------------------------------------------
    property var    services:   []
    property var    expanded:   ({})   // map: serviceIndex -> bool

    function rebuild() {
        try {
            gattPage.services = JSON.parse(backend.gattTableJson)
        } catch (e) {
            gattPage.services = []
        }
    }

    Component.onCompleted: rebuild()

    Connections {
        target: backend
        function onGattTableChanged() { gattPage.rebuild() }
    }

    // -----------------------------------------------------------------------
    // Property → colour map (nRF-Connect-style chips)
    // -----------------------------------------------------------------------
    function propColor(p) {
        switch (p) {
        case "read":            return "#3B82F6"
        case "write":           return "#F59E0B"
        case "write-without-response": return "#F59E0B"
        case "notify":          return "#10B981"
        case "indicate":        return "#10B981"
        default:                return "#64748B"
        }
    }
    function propShort(p) {
        switch (p) {
        case "read":            return "R"
        case "write":           return "W"
        case "write-without-response": return "WNR"
        case "notify":          return "N"
        case "indicate":        return "I"
        default:                return p.toUpperCase().substring(0, 3)
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
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Connect to a device on the Scan page to inspect its GATT services."; color: "#64748B"; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter }
        }
    }

    // -----------------------------------------------------------------------
    // Layout
    // -----------------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---------------------------------------------------------------
        // Header: connection summary + refresh
        // ---------------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: hdrCol.implicitHeight + 28
            color: "#0D1628"

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width; height: 1
                color: Qt.rgba(1, 1, 1, 0.06)
            }

            RowLayout {
                anchors { fill: parent; leftMargin: 24; rightMargin: 16; topMargin: 14; bottomMargin: 14 }

                ColumnLayout {
                    id: hdrCol
                    spacing: 4
                    Layout.fillWidth: true

                    Text {
                        text: "GATT Service Explorer"
                        color: "#F1F5F9"
                        font.pixelSize: 18
                        font.weight: Font.Medium
                    }
                    RowLayout {
                        spacing: 16
                        Text {
                            text: "Device: " + (backend && backend.connected
                                  ? (backend.deviceName !== "" ? backend.deviceName : "Connected")
                                  : "—")
                            color: "#94A3B8"; font.pixelSize: 12
                        }
                        Text {
                            text: "Address: " + (backend ? backend.connectedAddress : "—")
                            color: "#94A3B8"; font.pixelSize: 12
                        }
                        Text {
                            text: "MTU: " + (backend && backend.connMtu > 0 ? backend.connMtu + " B" : "—")
                            color: "#94A3B8"; font.pixelSize: 12
                        }
                        Text {
                            text: gattPage.services.length + " services"
                            color: "#94A3B8"; font.pixelSize: 12
                        }
                    }
                }

                Button {
                    text: (backend && backend.gattScanning) ? "Scanning…" : "↻  Refresh"
                    enabled: backend && backend.connected && !backend.gattScanning
                    onClicked: backend.refreshGattTable()

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
                    implicitWidth: 110
                    implicitHeight: 36
                }
            }
        }

        // ---------------------------------------------------------------
        // Service list
        // ---------------------------------------------------------------
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: gattPage.width
                spacing: 0

                // Empty state
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    visible: gattPage.services.length === 0
                    Text {
                        anchors.centerIn: parent
                        text: (backend && backend.gattScanning)
                              ? "Discovering services…"
                              : "No services discovered. Press Refresh."
                        color: "#475569"; font.pixelSize: 13
                    }
                }

                Repeater {
                    model: gattPage.services

                    delegate: Rectangle {
                        id: svcCard
                        required property int index
                        required property var modelData

                        property bool isOpen: gattPage.expanded[index] === undefined
                                              ? true   // services open by default
                                              : gattPage.expanded[index]

                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        Layout.rightMargin: 16
                        Layout.topMargin: 12
                        Layout.preferredHeight: cardCol.implicitHeight + 24
                        radius: 10
                        color: Qt.rgba(1, 1, 1, 0.038)
                        border.color: Qt.rgba(1, 1, 1, 0.08)

                        ColumnLayout {
                            id: cardCol
                            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
                            spacing: 8

                            // Service header (clickable to collapse)
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    text: svcCard.isOpen ? "▾" : "▸"
                                    color: "#64748B"; font.pixelSize: 14
                                }
                                Rectangle {
                                    width: 8; height: 8; radius: 4
                                    color: "#3B82F6"
                                    Layout.alignment: Qt.AlignVCenter
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Text {
                                        text: svcCard.modelData.name
                                        color: "#F1F5F9"; font.pixelSize: 14; font.weight: Font.Medium
                                    }
                                    Text {
                                        text: svcCard.modelData.uuid
                                        color: "#475569"; font.pixelSize: 10; font.family: "Consolas, monospace"
                                    }
                                }
                                Text {
                                    text: svcCard.modelData.characteristics.length + " chars"
                                    color: "#64748B"; font.pixelSize: 11
                                }

                                TapHandler {
                                    onTapped: {
                                        var m = gattPage.expanded
                                        m[svcCard.index] = !svcCard.isOpen
                                        gattPage.expanded = m
                                    }
                                }
                            }

                            // Characteristics
                            Repeater {
                                model: svcCard.isOpen ? svcCard.modelData.characteristics : []

                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 22
                                    Layout.preferredHeight: chrCol.implicitHeight + 16
                                    radius: 6
                                    color: Qt.rgba(1, 1, 1, 0.025)
                                    border.color: Qt.rgba(1, 1, 1, 0.05)

                                    ColumnLayout {
                                        id: chrCol
                                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
                                        spacing: 5

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 1
                                                Text {
                                                    text: modelData.name
                                                    color: "#CBD5E1"; font.pixelSize: 13; font.weight: Font.Medium
                                                }
                                                Text {
                                                    text: modelData.uuid
                                                    color: "#475569"; font.pixelSize: 10; font.family: "Consolas, monospace"
                                                }
                                            }

                                            // Property chips
                                            Row {
                                                spacing: 4
                                                Repeater {
                                                    model: modelData.properties
                                                    delegate: Rectangle {
                                                        required property var modelData
                                                        width: chipTxt.implicitWidth + 12
                                                        height: 18
                                                        radius: 4
                                                        color: Qt.rgba(0, 0, 0, 0)
                                                        border.color: gattPage.propColor(modelData)
                                                        border.width: 1
                                                        Text {
                                                            id: chipTxt
                                                            anchors.centerIn: parent
                                                            text: gattPage.propShort(modelData)
                                                            color: gattPage.propColor(modelData)
                                                            font.pixelSize: 10; font.weight: Font.Bold
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        // Value (if read)
                                        Rectangle {
                                            visible: modelData.value_hex !== null && modelData.value_hex !== ""
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: valCol.implicitHeight + 10
                                            radius: 4
                                            color: Qt.rgba(0, 0, 0, 0.25)

                                            ColumnLayout {
                                                id: valCol
                                                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; leftMargin: 8; rightMargin: 8 }
                                                spacing: 1
                                                Text {
                                                    visible: modelData.value_text !== null && modelData.value_text !== ""
                                                    text: "“" + (modelData.value_text || "") + "”"
                                                    color: "#93C5FD"; font.pixelSize: 11
                                                    wrapMode: Text.WrapAnywhere
                                                    Layout.fillWidth: true
                                                }
                                                Text {
                                                    text: "0x" + (modelData.value_hex || "")
                                                    color: "#64748B"; font.pixelSize: 10; font.family: "Consolas, monospace"
                                                    wrapMode: Text.WrapAnywhere
                                                    Layout.fillWidth: true
                                                }
                                            }
                                        }

                                        // Descriptors
                                        Repeater {
                                            model: modelData.descriptors
                                            delegate: Text {
                                                required property var modelData
                                                Layout.leftMargin: 6
                                                text: "↳ " + modelData.name + "  (" + modelData.uuid + ")"
                                                color: "#475569"; font.pixelSize: 10
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true; Layout.preferredHeight: 20 }
            }
        }
    }
}
