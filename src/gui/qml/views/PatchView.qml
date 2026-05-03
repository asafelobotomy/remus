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