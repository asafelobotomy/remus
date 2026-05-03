import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

Frame {
    required property string title
    required property int progressValue
    required property int progressTotal
    property string message: ""

    Layout.fillWidth: true

    background: Rectangle {
        radius: 16
        color: "#32302f"
        border.color: "#504945"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            text: title
            font.bold: true
            color: "#fbf1c7"
        }

        ProgressBar {
            Layout.fillWidth: true
            from:              0
            to:                Math.max(1, progressTotal)
            value:             progressValue
            // Show a pulsing bar while the total is unknown (e.g. during scan)
            indeterminate:     progressTotal <= 0
        }

        Label {
            Layout.fillWidth: true
            color: "#a89984"
            text: message.length > 0 ? message : progressValue + " / " + progressTotal
        }
    }
}