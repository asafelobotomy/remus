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
            text: "Hash Calculation"
            font.pixelSize: 26
            font.bold: true
        }

        RowLayout {
            Button {
                text: "Hash All Unhashed"
                enabled: !hashController.hashing
                onClicked: hashController.startHashAll()
            }

            Button {
                text: "Hash Selected"
                enabled: !hashController.hashing && appController.selectedFileId > 0
                onClicked: hashController.hashSelected()
            }
        }

        ProgressCard {
            title: "Hash Progress"
            progressValue: hashController.hashedFiles
            progressTotal: hashController.totalFiles
            message: hashController.hashing ? "Hashing files..." : "Ready"
        }
    }
}