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
            text: "Conversion"
            font.pixelSize: 26
            font.bold: true
        }

        RowLayout {
            ComboBox {
                id: formatBox
                model: ["CHD", "RVZ", "CSO", "WBFS", "PBP"]
                onActivated: conversionController.targetFormat = currentText
            }

            TextField {
                id: conversionOutputField
                Layout.fillWidth: true
                placeholderText: "Optional output path"
            }

            Button {
                text: "Convert"
                enabled: !conversionController.converting && appController.selectedFileId > 0
                onClicked: conversionController.convertSelected(formatBox.currentText, conversionOutputField.text)
            }
            Button {
                text: "Refresh Tool Status"
                onClicked: conversionController.refreshToolStatus()
            }
        }

        ProgressCard {
            title: "Conversion Progress"
            progressValue: conversionController.progress
            progressTotal: 100
            message: conversionController.lastMessage
        }

        Frame {
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                Label { text: "chdman: " + (conversionController.toolStatus.chdman ? "ready" : "missing") }
                Label { text: "dolphin-tool: " + (conversionController.toolStatus.dolphinTool ? "ready" : "missing") }
                Label { text: "maxcso: " + (conversionController.toolStatus.maxcso ? "ready" : "missing") }
                Label { text: "wit: " + (conversionController.toolStatus.wit ? "ready" : "missing") }
                Label { text: "PSXPackager: " + (conversionController.toolStatus.psxpackager ? "ready" : "missing") }
                Label {
                    text: conversionController.lastOutputPath.length > 0
                        ? "Output: " + conversionController.lastOutputPath
                        : ""
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}