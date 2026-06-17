import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

Frame {
    id: sidebar

    required property int currentIndex
    signal viewRequested(int index)

    readonly property var items: [
        { label: "Library",  index: 0, icon: "folder-symbolic" },
        { label: "Settings", index: 1, icon: "preferences-system-symbolic" }
    ]

    background: Rectangle {
        radius: 20
        color: Theme.surfaceAlt
        border.color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Label {
            text: "Remus"
            font.pixelSize: Theme.fontHero
            font.bold: true
            color: Theme.textPrimary
        }

        Repeater {
            model: sidebar.items

            Button {
                required property var modelData

                Layout.fillWidth: true
                checkable: true
                checked: sidebar.currentIndex === modelData.index
                text: modelData.label
                display: AbstractButton.TextBesideIcon
                icon.name: modelData.icon
                icon.width: 18
                icon.height: 18
                icon.color: checked ? Theme.textPrimary : Theme.textMuted
                opacity: (modelData.index === 1 || appController.libraryOpen) ? 1.0 : 0.38
                onClicked: sidebar.viewRequested(modelData.index)
            }
        }

        Item { Layout.fillHeight: true }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textMuted
            font.pixelSize: Theme.fontSm
            text: appController.libraryOpen
                  ? appController.statusMessage
                  : "Open a library database, then set your ROM folder in Settings."
        }
    }
}
