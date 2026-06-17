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
        color: Theme.surfaceAlt
        border.color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            text: title
            font.bold: true
            color: Theme.textPrimary
        }

        // Unified custom bar — consistent across all pipeline stages
        Rectangle {
            Layout.fillWidth: true
            height:           6
            radius:           3
            color:            Theme.borderSub
            clip:             true

            // Determinate fill
            Rectangle {
                visible: progressTotal > 0
                width:   (progressValue / Math.max(1, progressTotal)) * parent.width
                height:  parent.height
                radius:  parent.radius
                color:   Theme.success

                Behavior on width {
                    NumberAnimation { duration: 150; easing.type: Easing.InOutSine }
                }
            }

            // Indeterminate sweep
            Rectangle {
                id:      sweeper
                visible: progressTotal <= 0
                width:   parent.width * 0.35
                height:  parent.height
                radius:  parent.radius
                color:   Theme.success
                x:       -width

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
            color: Theme.textMuted
            text: message.length > 0 ? message : progressValue + " / " + progressTotal
        }
    }
}
