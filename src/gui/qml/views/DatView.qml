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
            text: "DAT Management"
            font.pixelSize: 26
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: datPathField
                Layout.fillWidth: true
                placeholderText: "/path/to/catalog.dat"
            }

            TextField {
                id: datSystemField
                Layout.preferredWidth: 180
                placeholderText: "System"
            }

            Button {
                text: "Import"
                enabled: !datManagerController.importing
                onClicked: datManagerController.importDat(datPathField.text, datSystemField.text)
            }
        }

        ProgressCard {
            title: "Import Progress"
            progressValue: datManagerController.progress
            progressTotal: datManagerController.total
            message: datManagerController.lastError.length > 0 ? datManagerController.lastError : "Ready"
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: datManagerController.loadedDats

            delegate: Frame {
                id: datDelegate

                required property var modelData

                width: ListView.view.width
                padding: 10
                implicitHeight: contentRow.implicitHeight + topPadding + bottomPadding

                RowLayout {
                    id: contentRow

                    x: datDelegate.leftPadding
                    y: datDelegate.topPadding
                    width: datDelegate.availableWidth
                    spacing: 8

                    Label { text: modelData.system; font.bold: true }
                    Label { Layout.fillWidth: true; text: modelData.name; elide: Text.ElideRight }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "Remove"
                        onClicked: datManagerController.removeDat(modelData.system)
                    }
                }
            }
        }
    }
}