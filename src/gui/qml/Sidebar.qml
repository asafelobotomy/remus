import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Frame {
    id: sidebar

    required property int currentIndex
    signal viewRequested(int index)

    readonly property var items: [
        { label: "Library",   index: 0 },
        { label: "Utilities", index: 1 },
        { label: "Settings",  index: 2 }
    ]

    background: Rectangle {
        radius: 20
        color: "#32302f"
        border.color: "#504945"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Label {
            text: "Remus"
            font.pixelSize: 26
            font.bold: true
            color: "#fbf1c7"
        }

        Repeater {
            model: sidebar.items

            Button {
                required property var modelData

                Layout.fillWidth: true
                checkable: true
                checked: sidebar.currentIndex === modelData.index
                text: modelData.label
                onClicked: sidebar.viewRequested(modelData.index)
            }
        }

        Item { Layout.fillHeight: true }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#a89984"
            text: appController.statusMessage.length > 0 ? appController.statusMessage : "Open a library database to begin."
        }
    }
}
