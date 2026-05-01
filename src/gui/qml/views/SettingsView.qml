import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

ScrollView {
    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout {
        width: parent.width
        spacing: 18

        Label {
            text: "Settings"
            font.pixelSize: 26
            font.bold: true
        }

        Label { text: "Metadata Providers"; font.bold: true }
        Repeater {
            model: settingsController.providerFields

            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true

                Label {
                    Layout.preferredWidth: 220
                    text: modelData.label
                }

                TextField {
                    Layout.fillWidth: true
                    echoMode: modelData.password ? TextInput.Password : TextInput.Normal
                    text: settingsController.stringValue(modelData.key)
                    onEditingFinished: settingsController.setValue(modelData.key, text)
                }
            }
        }

        Label { text: "Tools and Paths"; font.bold: true }
        Repeater {
            model: settingsController.toolFields

            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true

                Label {
                    Layout.preferredWidth: 220
                    text: modelData.label
                }

                TextField {
                    Layout.fillWidth: true
                    text: settingsController.stringValue(modelData.key)
                    onEditingFinished: settingsController.setValue(modelData.key, text)
                }
            }
        }

        Button {
            text: "Reset Saved Settings"
            onClicked: settingsController.resetToDefaults()
        }
    }
}