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
            visible:          progressTotal > 0
            Layout.fillWidth: true
            from:             0
            to:               Math.max(1, progressTotal)
            value:            progressValue
        }

        // Custom animated sweep when total is unknown (Fusion style ignores indeterminate)
        Rectangle {
            visible:          progressTotal <= 0
            Layout.fillWidth: true
            height:           6
            radius:           3
            color:            "#3c3836"
            clip:             true

            Rectangle {
                id:     sweeper
                width:  parent.width * 0.35
                height: parent.height
                radius: parent.radius
                color:  "#689d6a"
                x:      -width

                SequentialAnimation on x {
                    running:  progressTotal <= 0
                    loops:    Animation.Infinite
                    NumberAnimation {
                        from:        -sweeper.width
                        to:          sweeper.parent.width
                        duration:    1100
                        easing.type: Easing.InOutSine
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            color: "#a89984"
            text: message.length > 0 ? message : progressValue + " / " + progressTotal
        }
    }
}