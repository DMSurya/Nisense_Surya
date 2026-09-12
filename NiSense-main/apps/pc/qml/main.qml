import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "."

ApplicationWindow {
    id: root
    title: "HCM Monitor"
    width: 1100
    height: 720
    minimumWidth: 780
    minimumHeight: 540
    visible: true
    color: Theme.bgPrimary

    // -----------------------------------------------------------------------
    // Navigation model
    // -----------------------------------------------------------------------
    readonly property var pages: [
        { title: "Scan",      key: "scan",      source: "ScanPage.qml"      },
        { title: "Dashboard", key: "dashboard", source: "DashboardPage.qml" },
        { title: "Charts",    key: "charts",    source: "ChartsPage.qml"    },
        { title: "Settings",  key: "settings",  source: "SettingsPage.qml"  },
        { title: "Services",  key: "services",  source: "GattPage.qml"      },
        { title: "Logs",      key: "logs",      source: "LogPage.qml"       },
        { title: "Firmware",  key: "firmware",  source: "DfuPage.qml"       },
    ]
    property int currentPage: 0

    // -----------------------------------------------------------------------
    // Header bar
    // -----------------------------------------------------------------------
    header: Rectangle {
        height: 52
        color: Theme.bgHeader

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width; height: 1
            color: Qt.rgba(1, 1, 1, 0.06)
        }

        RowLayout {
            anchors { fill: parent; leftMargin: 16; rightMargin: 16 }

            // App logo + title
            Row {
                spacing: 10
                Rectangle {
                    width: 30; height: 30; radius: 8
                    color: Theme.bgElevated
                    border.color: Theme.borderMid
                    border.width: 1
                    anchors.verticalCenter: parent.verticalCenter
                    Image {
                        anchors.centerIn: parent
                        width: 24; height: 24
                        source: Theme.logo
                        sourceSize.width: 48; sourceSize.height: 48
                        fillMode: Image.PreserveAspectFit
                        smooth: true; mipmap: true; asynchronous: true
                    }
                }
                Text {
                    text: "HCM Monitor"
                    color: Theme.textPrimary
                    font.pixelSize: 16; font.weight: Font.Normal
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Item { Layout.fillWidth: true }

            // Connection pill
            Rectangle {
                height: 30
                width: pillRow.implicitWidth + 22
                radius: 15
                color: (backend && backend.connected)
                       ? Qt.rgba(0.063, 0.725, 0.506, 0.14)
                       : Qt.rgba(0.937, 0.267, 0.267, 0.10)
                border.color: (backend && backend.connected)
                              ? Qt.rgba(0.063, 0.725, 0.506, 0.40)
                              : Qt.rgba(0.937, 0.267, 0.267, 0.28)
                Behavior on color        { ColorAnimation { duration: 300 } }
                Behavior on border.color { ColorAnimation { duration: 300 } }

                Row {
                    id: pillRow
                    anchors.centerIn: parent
                    spacing: 8

                    Item {
                        width: 10; height: 10
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            anchors.centerIn: parent
                            color: (backend && backend.connected) ? "#10B981" : "#EF4444"
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                        Rectangle {
                            width: 10; height: 10; radius: 5
                            anchors.centerIn: parent
                            color: "transparent"
                            border.color: (backend && backend.connected) ? "#10B981" : "transparent"
                            border.width: 1.5
                            opacity: 0
                            SequentialAnimation on opacity {
                                running: backend && backend.connected
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.7; duration: 700 }
                                NumberAnimation { to: 0.0; duration: 700 }
                            }
                        }
                    }

                    Text {
                        text: (backend && backend.connected)
                              ? (backend.deviceName !== "" ? backend.deviceName : "Connected")
                              : "Disconnected"
                        color: (backend && backend.connected) ? "#6EE7B7" : "#FCA5A5"
                        font.pixelSize: 13; font.weight: Font.Normal
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on color { ColorAnimation { duration: 300 } }
                    }
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Body: icon-only nav rail + page content
    // -----------------------------------------------------------------------
    Row {
        anchors.fill: parent

        // Nav rail (larger touch targets + icons)
        Rectangle {
            id: navRail
            width: 68; height: parent.height
            color: Theme.bgNav

            Rectangle {
                anchors.right: parent.right
                anchors.top:   parent.top
                anchors.bottom:parent.bottom
                width: 1; color: Qt.rgba(1, 1, 1, 0.05)
            }

            Column {
                anchors.top: parent.top
                anchors.topMargin: 14
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Repeater {
                    model: root.pages
                    delegate: Item {
                        width: 60; height: 60

                        // Active left-edge bar
                        Rectangle {
                            anchors.left:           parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: 3
                            height: root.currentPage === index ? 30 : 0
                            radius: 2; color: "#3B82F6"
                            Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 46; height: 46; radius: 12
                            color: root.currentPage === index
                                   ? Qt.rgba(0.231, 0.510, 0.965, 0.20)
                                   : (navHover.containsMouse ? Qt.rgba(1,1,1,0.06) : "transparent")
                            Behavior on color { ColorAnimation { duration: 140 } }

                            Image {
                                anchors.centerIn: parent
                                width: 22; height: 22
                                source: Theme.navIcon(modelData.key, root.currentPage === index)
                                sourceSize.width: 44; sourceSize.height: 44
                                smooth: true; asynchronous: true
                            }
                        }

                        HoverHandler { id: navHover }
                        TapHandler  { onTapped: root.currentPage = index }

                        ToolTip {
                            visible: navHover.hovered
                            text:    modelData.title
                            delay:   450
                        }
                    }
                }
            }
        }

        // Page content area
        Item {
            width: parent.width - navRail.width
            height: parent.height
            clip: true

            StackLayout {
                anchors.fill: parent
                currentIndex: root.currentPage
                clip: true

                Loader { source: "ScanPage.qml"     }
                Loader { source: "DashboardPage.qml"}
                Loader { source: "ChartsPage.qml"   }
                Loader { source: "SettingsPage.qml" }
                Loader { source: "GattPage.qml"     }
                Loader { source: "LogPage.qml"      }
                Loader { source: "DfuPage.qml"      }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Toast notification bar — anchored bottom-right, z-floats above pages
    // -----------------------------------------------------------------------
    Item {
        id: toastAnchor
        anchors {
            right:  parent.right
            bottom: parent.bottom
            rightMargin:  20
            bottomMargin: 20
        }
        width: 340
        height: toastRect.height
        z: 9999

        property var    _queue: []
        property bool   _showing: false
        property color  _accent: "#3B82F6"

        function show(msg, level) {
            _queue.push({ msg: msg, level: level || "info" })
            if (!_showing) _dequeue()
        }
        function _dequeue() {
            if (_queue.length === 0) { _showing = false; return }
            _showing = true
            var item = _queue.shift()
            toastText.text = item.msg
            toastIcon.text = item.level === "error"   ? "⚠" :
                             item.level === "success" ? "✓" :
                             item.level === "warning" ? "!" : "i"
            _accent = item.level === "error"   ? "#EF4444" :
                      item.level === "success" ? "#10B981" :
                      item.level === "warning" ? "#F59E0B" : "#3B82F6"
            toastRect.opacity = 0; toastRect.y = 12
            showAnim.start(); dismissTimer.restart()
        }

        Rectangle {
            id: toastRect
            width: parent.width
            height: toastContent.implicitHeight + 20
            radius: 10
            color: Qt.rgba(0.055, 0.086, 0.18, 0.96)
            border.color: Qt.rgba(toastAnchor._accent.r, toastAnchor._accent.g, toastAnchor._accent.b, 0.42)
            border.width: 1
            opacity: 0
            visible: toastAnchor._showing

            // Left accent stripe
            Rectangle {
                x: 10; width: 3
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height - 16; radius: 2
                color: toastAnchor._accent
            }

            Row {
                id: toastContent
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                anchors.leftMargin: 22; anchors.rightMargin: 14
                spacing: 10

                Text {
                    id: toastIcon
                    text: "i"; color: toastAnchor._accent
                    font.pixelSize: 14; font.weight: Font.Bold
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    id: toastText
                    text: ""; color: "#E2E8F0"
                    font.pixelSize: 13; wrapMode: Text.WordWrap
                    width: toastContent.width - toastIcon.width - 20
                }
            }

            ParallelAnimation {
                id: showAnim
                NumberAnimation { target: toastRect; property: "opacity"; from: 0; to: 1;  duration: 220; easing.type: Easing.OutCubic }
                NumberAnimation { target: toastRect; property: "y";       from: 12; to: 0; duration: 220; easing.type: Easing.OutCubic }
            }
        }

        Timer {
            id: dismissTimer
            interval: 3200
            onTriggered: {
                hideAnim.start()
            }
        }

        NumberAnimation {
            id: hideAnim
            target: toastRect; property: "opacity"; to: 0
            duration: 280; easing.type: Easing.InCubic
            onStopped: {
                if (toastAnchor._queue.length > 0) toastAnchor._dequeue()
                else toastAnchor._showing = false
            }
        }
    }

    // -----------------------------------------------------------------------
    // Global pairing confirm dialog (numeric comparison) — any page
    // -----------------------------------------------------------------------
    property string pairingPasskey: ""
    // True once the user has tapped Accept here, while we wait for the
    // ceremony to settle (or the watch's confirm + bond to complete).
    property bool pairingBusy: false

    Dialog {
        id: pairingDialog
        modal: false
        anchors.centerIn: parent
        width: 340
        closePolicy: Popup.CloseOnEscape
        z: 10000
        onOpened: root.pairingBusy = false
        onClosed: root.pairingBusy = false
        background: Rectangle {
            color: "#111827"
            radius: 12
            border.color: "#1E3A8A"
            border.width: 1
        }

        contentItem: Column {
            spacing: 16
            padding: 20
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Confirm Pairing"
                color: "#F1F5F9"
                font.pixelSize: 18
                font.bold: true
            }
            Text {
                width: 300
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                text: root.pairingBusy
                      ? "Waiting for bonding to finish…"
                      : "1) Verify the code matches the watch.\n2) Accept on either device (either order works).\n3) If the watch Accepts first, PC confirms automatically."
                color: "#94A3B8"
                font.pixelSize: 13
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.pairingPasskey
                color: "#60A5FA"
                font.pixelSize: 40
                font.bold: true
                font.letterSpacing: 6
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 14

                Rectangle {
                    width: 110
                    height: 40
                    radius: 8
                    opacity: root.pairingBusy ? 0.5 : 1.0
                    color: rejectMA.pressed ? Qt.rgba(0.12, 0.16, 0.22, 1)
                         : rejectMA.containsMouse ? "#374151" : "#1F2937"
                    Text {
                        anchors.centerIn: parent
                        text: "Close"
                        color: "#94A3B8"
                        font.pixelSize: 14
                    }
                    MouseArea {
                        id: rejectMA
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: !root.pairingBusy
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (backend) backend.dismissPairingDialog()
                            pairingDialog.close()
                        }
                    }
                }

                Rectangle {
                    width: 110
                    height: 40
                    radius: 8
                    opacity: root.pairingBusy ? 0.6 : 1.0
                    color: acceptMA.pressed ? "#1D4ED8" : acceptMA.containsMouse ? "#3B82F6" : "#2563EB"
                    Text {
                        anchors.centerIn: parent
                        text: root.pairingBusy ? "Pairing…" : "Accept"
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        font.bold: true
                    }
                    MouseArea {
                        id: acceptMA
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: !root.pairingBusy
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (backend) backend.confirmPairing(true)
                            root.pairingBusy = true
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: backend
        function onStatusChanged(msg)  { toastAnchor.show(msg, "info")  }
        function onErrorOccurred(msg)  { toastAnchor.show(msg, "error") }
        function onScanComplete(count) {
            if (count > 0) toastAnchor.show("Found " + count + " device(s)", "success")
            else           toastAnchor.show("No HCM devices found", "warning")
        }
        function onPairingPasskey(code) {
            if (!code || code.length === 0) {
                root.pairingPasskey = ""
                pairingDialog.close()
                return
            }
            root.pairingPasskey = code
            root.pairingBusy = false
            pairingDialog.open()
        }
        function onPairingStateChanged(state) {
            if (state === "confirm" && root.pairingPasskey.length > 0) {
                if (!pairingDialog.opened)
                    pairingDialog.open()
            }
            if (state === "pairing" || state === "idle" || state === "paired" || state === "failed") {
                root.pairingBusy = false
                root.pairingPasskey = ""
                pairingDialog.close()
            }
        }
        function onConnectionStateChanged(connected) {
            if (!connected) {
                root.pairingBusy = false
                root.pairingPasskey = ""
                pairingDialog.close()
            }
        }
        function onIsConnectingChanged() {
            // Connect/pair recovery finished without bond — drop stale overlay.
            if (backend && !backend.isConnecting && !backend.connected
                    && backend.pairingState !== "confirm") {
                root.pairingBusy = false
                root.pairingPasskey = ""
                pairingDialog.close()
            }
        }
    }
}

