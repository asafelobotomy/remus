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
            text: "Organize"
            font.pixelSize: 26
            font.bold: true
        }

        TextField {
            id: templateField
            Layout.fillWidth: true
            placeholderText: "Naming template"
            text: organizeController.namingTemplate
            onEditingFinished: organizeController.namingTemplate = text
        }

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: organizeDestinationField
                Layout.fillWidth: true
                placeholderText: "/path/to/organized-output"
            }

            Button {
                text: "Preview"
                enabled: !organizeController.organizing
                onClicked: organizeController.previewOrganize(organizeDestinationField.text)
            }
            Button {
                text: "Apply"
                enabled: !organizeController.organizing
                onClicked: organizeController.applyOrganize(organizeDestinationField.text)
            }
            Button {
                text: "Undo"
                onClicked: organizeController.undoLast()
            }
        }

        Label {
            color: "#fb4934"
            text: organizeController.lastError
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: organizeController.previewEntries

            delegate: Frame {
                id: organizeDelegate

                required property var modelData

                width: ListView.view.width
                padding: 12
                implicitHeight: contentColumn.implicitHeight + topPadding + bottomPadding

                ColumnLayout {
                    id: contentColumn

                    x: organizeDelegate.leftPadding
                    y: organizeDelegate.topPadding
                    width: organizeDelegate.availableWidth
                    spacing: 4

                    Label { Layout.fillWidth: true; text: modelData.success ? "Ready" : "Failed"; font.bold: true }
                    Label { Layout.fillWidth: true; text: modelData.oldPath; elide: Text.ElideMiddle }
                    Label { Layout.fillWidth: true; text: modelData.newPath; elide: Text.ElideMiddle }
                    Label {
                        visible: modelData.error && modelData.error.length > 0
                        Layout.fillWidth: true
                        color: "#fb4934"
                        wrapMode: Text.WordWrap
                        text: modelData.error
                    }
                }
            }
        }
    }
}