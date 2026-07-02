import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

Rectangle {
    id: banner

    property alias message: messageLabel.text

    visible: message.length > 0
    height: visible ? implicitHeight : 0
    radius: 8
    color: Theme.error
    border.color: Theme.border

    implicitHeight: row.implicitHeight + 20

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        Label {
            id: messageLabel

            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSm
        }

        ToolButton {
            text: "\u2715"
            onClicked: appController.dismissError()
            Accessible.name: "Dismiss error"
        }
    }
}
