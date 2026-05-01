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
            text: "Scan ROM Directories"
            font.pixelSize: 26
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: scanPathField
                Layout.fillWidth: true
                placeholderText: "/path/to/roms"
                text: scanController.lastDirectory
                onEditingFinished: scanController.lastDirectory = text
            }

            Button {
                text: "Start"
                enabled: !scanController.scanning
                onClicked: scanController.startScan(scanPathField.text)
            }

            Button {
                text: "Cancel"
                enabled: scanController.scanning
                onClicked: scanController.stopScan()
            }
        }

        ProgressCard {
            title: "Scan Progress"
            progressValue: scanController.scannedFiles
            progressTotal: scanController.totalFiles
            message: scanController.recentLogs.length > 0 ? scanController.recentLogs[scanController.recentLogs.length - 1] : "Idle"
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                anchors.fill: parent
                model: scanController.recentLogs
                delegate: Label {
                    required property string modelData
                    width: ListView.view.width
                    text: modelData
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}