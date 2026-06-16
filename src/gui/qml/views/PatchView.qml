import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Remus.Gui

Item {
    Layout.fillWidth: true
    Layout.fillHeight: true

    FileDialog {
        id: modifiedRomDialog
        title: "Select modified ROM"
        onAccepted: {
            modifiedPathField.text = decodeURIComponent(selectedFile.toString().replace(/^file:\/\//, ""))
        }
    }

    FileDialog {
        id: patchOutputDialog
        title: "Save patch file"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Patch files (*.bps *.ips *.ups *.xdelta *.ppf)"]
        onAccepted: {
            patchOutputField.text = decodeURIComponent(selectedFile.toString().replace(/^file:\/\//, ""))
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        Label {
            text: "Patch Application"
            font.pixelSize: 26
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "Base ROM: " + (appController.selectedFileData.path || "Select a file in the library view.")
        }

        TextField {
            id: patchPathField
            Layout.fillWidth: true
            placeholderText: "/path/to/patch.(ips|bps|ups|xdelta|ppf)"
        }

        TextField {
            id: patchedOutputField
            Layout.fillWidth: true
            placeholderText: "Optional output path"
        }

        RowLayout {
            Button {
                text: "Apply Patch"
                enabled: !patchController.patching && appController.selectedFileId > 0
                onClicked: patchController.applyPatch("", patchPathField.text, patchedOutputField.text)
            }
            Button {
                text: "Refresh Tool Status"
                onClicked: patchController.checkTools()
            }
        }

        Label {
            text: "Create Patch"
            font.bold: true
            font.pixelSize: 14
        }

        TextField {
            id: modifiedPathField
            Layout.fillWidth: true
            placeholderText: "Modified ROM path"
        }

        TextField {
            id: patchOutputField
            Layout.fillWidth: true
            placeholderText: "Output patch path"
        }

        RowLayout {
            Label { text: "Format" }
            ComboBox {
                id: patchFormatCombo
                model: ["bps", "ips", "ups", "xdelta"]
            }
            Button {
                text: "Browse modified…"
                flat: true
                onClicked: modifiedRomDialog.open()
            }
            Button {
                text: "Browse output…"
                flat: true
                onClicked: patchOutputDialog.open()
            }
            Button {
                text: "Create Patch"
                enabled: !patchController.patching
                        && appController.selectedFileData.path
                        && modifiedPathField.text.length > 0
                        && patchOutputField.text.length > 0
                onClicked: patchController.createPatch(
                    appController.selectedFileData.path,
                    modifiedPathField.text,
                    patchOutputField.text,
                    patchFormatCombo.currentText)
            }
        }

        ProgressCard {
            title: "Patch Progress"
            progressValue: patchController.progress
            progressTotal: 100
            message: patchController.currentOperation
        }

        Frame {
            Layout.fillWidth: true
            ColumnLayout {
                anchors.fill: parent
                Label { text: "IPS: " + (patchController.toolStatus.ips ? "ready" : "missing") }
                Label { text: "BPS/UPS: " + (patchController.toolStatus.bps ? "ready" : "missing") }
                Label { text: "xdelta3: " + (patchController.toolStatus.xdelta3 ? "ready" : "missing") }
                Label { text: "PPF: " + (patchController.toolStatus.ppf ? "ready" : "missing") }
            }
        }
    }
}
