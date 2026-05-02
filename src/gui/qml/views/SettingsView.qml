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

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#504945"
        }

        Label {
            text: "Danger Zone"
            font.bold: true
            color: "#fb4934"
        }

        Label {
            Layout.fillWidth: true
            text: "Erase Library Database permanently removes all imported file records, matches, and metadata from the local library database. ROM files on disk are not affected."
            wrapMode: Text.WordWrap
            color: "#a89984"
        }

        Button {
            id: eraseButton
            text: "Erase Library Database"
            enabled: appController.libraryOpen
            palette.button: enabled ? "#cc241d" : "#504945"
            palette.buttonText: "#fbf1c7"
            onClicked: eraseConfirmDialog.open()
        }

        Dialog {
            id: eraseConfirmDialog
            title: "Erase Library Database?"
            modal: true
            standardButtons: Dialog.Ok | Dialog.Cancel
            anchors.centerIn: Overlay.overlay

            Label {
                text: "This will permanently erase all imported data from the library database.\nROM files on disk will not be deleted.\n\nThis action cannot be undone."
                wrapMode: Text.WordWrap
                width: 360
            }

            onAccepted: appController.eraseLibraryDatabase()
        }
    }
}