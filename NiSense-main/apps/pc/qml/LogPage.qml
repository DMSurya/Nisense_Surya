import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.folderlistmodel 2.15

Page {
    id: logPage
    background: Rectangle { color: "#0A0E1A" }

    // logDirPath is provided as a QML context property from Python
    // e.g. engine.rootContext().setContextProperty("logDirPath", Qt.url("file:///..."))

    // -----------------------------------------------------------------------
    // File model — newest first, CSV + JSON only
    // -----------------------------------------------------------------------
    FolderListModel {
        id: folderModel
        folder: typeof logDirPath !== "undefined" ? logDirPath : ""
        nameFilters: ["*.csv", "*.json", "*.txt"]
        sortField: FolderListModel.Time
        sortReversed: true
        showDirs: false
        showHidden: false
    }

    // -----------------------------------------------------------------------
    // Layout
    // -----------------------------------------------------------------------
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // Header row
        RowLayout {
            Text { text: "Log Files"; color: "#F1F5F9"; font.pixelSize: 22; font.weight: Font.Bold }
            Item { Layout.fillWidth: true }

            // File count badge
            Rectangle {
                height: 26; width: cntBadge.implicitWidth + 18; radius: 13
                color: Qt.rgba(0.231,0.510,0.965,0.16)
                border.color: Qt.rgba(0.231,0.510,0.965,0.36)
                visible: folderModel.count > 0
                Text { id: cntBadge; anchors.centerIn: parent; text: folderModel.count + " file" + (folderModel.count === 1 ? "" : "s"); color: "#93C5FD"; font.pixelSize: 12 }
            }

            // Open folder button
            Rectangle {
                height: 34; width: openFolderRow.implicitWidth + 22; radius: 8
                color: openFolderMA.pressed ? Qt.rgba(0.231,0.510,0.965,0.30)
                     : openFolderMA.containsMouse ? Qt.rgba(0.231,0.510,0.965,0.18)
                     : Qt.rgba(0.231,0.510,0.965,0.12)
                border.color: Qt.rgba(0.231,0.510,0.965,0.45)
                Behavior on color { ColorAnimation { duration: 130 } }
                Row { id: openFolderRow; anchors.centerIn: parent; spacing: 7
                    Text { text: "⊞"; color: "#93C5FD"; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: "Open Folder"; color: "#93C5FD"; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                }
                MouseArea { id: openFolderMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof logDirPath !== "undefined")
                            Qt.openUrlExternally(logDirPath)
                    }
                }
            }

            // Cloud upload (gateway.json)
            Rectangle {
                height: 34; width: uploadRow.implicitWidth + 22; radius: 8
                color: uploadMA.pressed ? Qt.rgba(0.063,0.725,0.506,0.30)
                     : uploadMA.containsMouse ? Qt.rgba(0.063,0.725,0.506,0.18)
                     : Qt.rgba(0.063,0.725,0.506,0.12)
                border.color: Qt.rgba(0.063,0.725,0.506,0.45)
                Behavior on color { ColorAnimation { duration: 130 } }
                Row { id: uploadRow; anchors.centerIn: parent; spacing: 7
                    Text { text: "↑"; color: "#6EE7B7"; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
                    Text { text: "Upload"; color: "#6EE7B7"; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                }
                MouseArea { id: uploadMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: { if (backend) backend.uploadSessionLogs() }
                }
            }
        }

        // Log directory path
        Text {
            text: typeof logDirPath !== "undefined" ? logDirPath.toString().replace("file:///","") : "Log directory not configured"
            color: "#475569"; font.pixelSize: 12
            font.family: "Consolas, monospace"
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }

        // ---------------------------------------------------------------
        // File list
        // ---------------------------------------------------------------
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true

            // Empty state
            Column {
                anchors.centerIn: parent; spacing: 12
                visible: folderModel.count === 0

                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "⊞"; font.pixelSize: 48; color: "#1E3A8A" }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "No log files yet"; color: "#475569"; font.pixelSize: 16; font.weight: Font.Normal }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "HR, SpO₂, Glucose and PPG streams are\nlogged automatically when notifications arrive."
                    color: "#334155"; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter
                }
            }

            ListView {
                id: fileList
                anchors.fill: parent
                clip: true; spacing: 8
                model: folderModel

                delegate: Rectangle {
                    width: fileList.width; height: 64; radius: 10
                    color: fileCardHover.hovered ? Qt.rgba(1,1,1,0.065) : Qt.rgba(1,1,1,0.040)
                    border.color: Qt.rgba(1,1,1,0.07)
                    Behavior on color { ColorAnimation { duration: 140 } }

                    HoverHandler { id: fileCardHover }

                    RowLayout {
                        anchors { fill: parent; leftMargin: 14; rightMargin: 10 }
                        spacing: 14

                        // Type badge
                        Rectangle {
                            width: 38; height: 38; radius: 8
                            color: Qt.rgba(typeColor.r, typeColor.g, typeColor.b, 0.18)
                            border.color: Qt.rgba(typeColor.r, typeColor.g, typeColor.b, 0.40)

                            property color typeColor: {
                                var n = fileName.toLowerCase()
                                if (n.indexOf("glucose") >= 0) return "#F59E0B"
                                if (n.indexOf("hr") >= 0 || n.indexOf("heart") >= 0) return "#EF4444"
                                if (n.indexOf("spo2") >= 0) return "#3B82F6"
                                if (n.indexOf("ppg") >= 0) return "#8B5CF6"
                                return "#64748B"
                            }

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    var n = fileName.toLowerCase()
                                    if (n.indexOf("glucose") >= 0) return "◆"
                                    if (n.indexOf("hr") >= 0 || n.indexOf("heart") >= 0) return "♥"
                                    if (n.indexOf("spo2") >= 0) return "○"
                                    if (n.indexOf("ppg") >= 0) return "∿"
                                    return "⊞"
                                }
                                font.pixelSize: 16; color: parent.typeColor
                            }
                        }

                        // Name + size + date
                        Column {
                            spacing: 3; Layout.fillWidth: true
                            Text {
                                text: fileName
                                color: "#F1F5F9"; font.pixelSize: 14; font.weight: Font.Normal
                                elide: Text.ElideRight; width: parent.width
                            }
                            Row { spacing: 10
                                Text { text: _fmtSize(fileSize); color: "#64748B"; font.pixelSize: 11 }
                                Text { text: "·"; color: "#334155"; font.pixelSize: 11 }
                                Text { text: fileModified.toLocaleDateString() + "  " + fileModified.toLocaleTimeString(Qt.locale(), "HH:mm"); color: "#64748B"; font.pixelSize: 11 }
                            }
                        }

                        // Action buttons
                        Row { spacing: 6; Layout.alignment: Qt.AlignVCenter
                            Rectangle {
                                height: 30; width: openBtnLbl.implicitWidth + 18; radius: 6
                                color: openBtnMA.pressed ? Qt.rgba(0.063,0.725,0.506,0.35)
                                     : openBtnMA.containsMouse ? Qt.rgba(0.063,0.725,0.506,0.22)
                                     : Qt.rgba(0.063,0.725,0.506,0.12)
                                border.color: Qt.rgba(0.063,0.725,0.506,0.50)
                                Behavior on color { ColorAnimation { duration: 120 } }
                                Text { id: openBtnLbl; anchors.centerIn: parent; text: "Open"; color: "#6EE7B7"; font.pixelSize: 12 }
                                MouseArea { id: openBtnMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: Qt.openUrlExternally(fileUrl)
                                }
                            }

                            // Delete button — with confirm state
                            Rectangle {
                                id: delBtn
                                property bool confirmState: false
                                height: 30; width: delLbl.implicitWidth + 18; radius: 6
                                color: delMA.pressed
                                       ? Qt.rgba(0.937,0.267,0.267,0.40)
                                       : delMA.containsMouse
                                         ? (confirmState ? Qt.rgba(0.937,0.267,0.267,0.45) : Qt.rgba(0.937,0.267,0.267,0.22))
                                         : (confirmState ? Qt.rgba(0.937,0.267,0.267,0.30) : Qt.rgba(0.937,0.267,0.267,0.10))
                                border.color: confirmState ? "#EF4444" : Qt.rgba(0.937,0.267,0.267,0.35)
                                Behavior on color { ColorAnimation { duration: 120 } }
                                Text { id: delLbl; anchors.centerIn: parent; text: delBtn.confirmState ? "Confirm" : "Delete"; color: delBtn.confirmState ? "#FCA5A5" : "#F87171"; font.pixelSize: 12 }
                                MouseArea { id: delMA; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (!delBtn.confirmState) { delBtn.confirmState = true; cancelTimer.restart() }
                                        else {
                                            delBtn.confirmState = false; cancelTimer.stop()
                                            backend.deleteLogFile(fileUrl.toString())
                                        }
                                    }
                                }
                                Timer { id: cancelTimer; interval: 3000; onTriggered: delBtn.confirmState = false }
                            }
                        }
                    }
                }
            }
        }
    }

    function _fmtSize(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB"
        return (bytes / 1048576).toFixed(2) + " MB"
    }
}


