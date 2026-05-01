import QtQuick
import QtQuick.Controls
import Remus.Gui

Rectangle {
    required property string text
    property color tone: "#b8bb26"

    implicitHeight: label.implicitHeight + 10
    implicitWidth: label.implicitWidth + 16
    radius: implicitHeight / 2
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.18)
    border.color: tone

    Label {
        id: label
        anchors.centerIn: parent
        color: tone
        font.pixelSize: 12
        text: parent.text
    }
}