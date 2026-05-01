import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

Item {
    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        Label {
            text: appController.libraryOpen ? "Library Contents" : "No library open"
            font.pixelSize: 26
            font.bold: true
            color: "#fbf1c7"
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 10
            model: fileListModel

            delegate: Frame {
                id: fileDelegate

                required property int fileId
                required property int gameId
                required property string displayName
                required property string systemName
                required property string matchedTitle
                required property double confidence
                required property string status
                required property bool matched
                required property bool confirmed
                required property bool rejected
                required property string path

                width: ListView.view.width
                padding: 12
                implicitHeight: contentColumn.implicitHeight + topPadding + bottomPadding

                background: Rectangle {
                    radius: 16
                    color: appController.selectedFileId === fileId ? "#3f4d4f" : "#32302f"
                    border.color: appController.selectedFileId === fileId ? "#83a598" : "#504945"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: appController.selectedFileId = fileId
                }

                ColumnLayout {
                    id: contentColumn

                    x: fileDelegate.leftPadding
                    y: fileDelegate.topPadding
                    width: fileDelegate.availableWidth
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: displayName
                        font.bold: true
                        font.pixelSize: 18
                        color: "#fbf1c7"
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        color: "#ebdbb2"
                        elide: Text.ElideRight
                        text: systemName + " • "
                            + (matched ? matchedTitle + " (" + Math.round(confidence) + "%)" : "Unmatched")
                            + " • " + status
                    }

                    Label {
                        Layout.fillWidth: true
                        color: "#a89984"
                        elide: Text.ElideMiddle
                        text: path
                    }
                }
            }
        }
    }
}