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
            text: "Metadata Matching"
            font.pixelSize: 26
            font.bold: true
        }

        RowLayout {
            Button {
                text: "Match Selected"
                enabled: !matchController.matching && appController.selectedFileId > 0
                onClicked: matchController.matchSelected()
            }
            Button {
                text: "Match All"
                enabled: !matchController.matching
                onClicked: matchController.matchAll()
            }
            Button {
                text: "Confirm"
                enabled: appController.selectedFileId > 0
                onClicked: matchController.confirmSelected()
            }
            Button {
                text: "Reject"
                enabled: appController.selectedFileId > 0
                onClicked: matchController.rejectSelected()
            }
        }

        Label {
            color: "#a89984"
            text: matchController.lastMessage.length > 0 ? matchController.lastMessage : "Run matching to populate game metadata."
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Frame {
                SplitView.preferredWidth: 360

                ListView {
                    anchors.fill: parent
                    clip: true
                    spacing: 10
                    model: matchListModel

                    delegate: Frame {
                        id: matchDelegate

                        required property int fileId
                        required property string title
                        required property string system
                        required property string fileName
                        required property double confidence
                        required property string method
                        required property bool confirmed
                        required property bool rejected

                        width: ListView.view.width
                        padding: 12
                        implicitHeight: contentColumn.implicitHeight + topPadding + bottomPadding

                        background: Rectangle {
                            radius: 14
                            color: appController.selectedFileId === fileId ? "#3f4d4f" : "#32302f"
                            border.color: appController.selectedFileId === fileId ? "#83a598" : "#504945"
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: appController.selectedFileId = fileId
                        }

                        ColumnLayout {
                            id: contentColumn

                            x: matchDelegate.leftPadding
                            y: matchDelegate.topPadding
                            width: matchDelegate.availableWidth
                            spacing: 6

                            Label { Layout.fillWidth: true; text: title; font.bold: true; elide: Text.ElideRight }
                            Label { Layout.fillWidth: true; text: fileName; elide: Text.ElideMiddle }
                            Label { Layout.fillWidth: true; text: system + " • " + Math.round(confidence) + "% • " + method; elide: Text.ElideRight }
                            Label {
                                text: confirmed ? "Confirmed" : (rejected ? "Rejected" : "Pending")
                                color: confirmed ? "#b8bb26" : (rejected ? "#fb4934" : "#fabd2f")
                            }
                        }
                    }
                }
            }

            MetadataEditor {
                SplitView.fillWidth: true
                SplitView.fillHeight: true
            }
        }
    }
}