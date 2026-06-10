import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

// TMM-style rename preview: bundle sidecars, then organize into library root.
Dialog {
    id: dialog

    title: "Rename & Organize"
    modal: true
    anchors.centerIn: Overlay.overlay
    width:  Math.min(760, Overlay.overlay.width * 0.9)
    height: Math.min(620, Overlay.overlay.height * 0.85)

    readonly property bool selectedOnly: appController.selectedFileId > 0
    readonly property string organizeRoot: settingsController.stringValue("gui/organize_destination", "")
    readonly property string libraryDest: organizeRoot.length > 0 ? organizeRoot + "/Remus Library" : ""
    readonly property int fileCount: organizeController.renameOrganizeFileCount(selectedOnly)

    onOpened: statusLabel.text = ""

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#928374"
            font.pixelSize: 11
            text: selectedOnly
                  ? "Selected ROM — bundle sidecars, then move into your organized library."
                  : "All confirmed matches — bundle sidecars, then move into your organized library."
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 4

            Label { text: "Naming template"; color: "#a89984"; font.pixelSize: 11 }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                text: organizeController.namingTemplate
                color: "#ebdbb2"
                font.pixelSize: 11
            }

            Label { text: "Organize into"; color: "#a89984"; font.pixelSize: 11 }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                text: libraryDest.length > 0 ? libraryDest : "Not configured — set in Settings"
                color: libraryDest.length > 0 ? "#ebdbb2" : "#fb4934"
                font.pixelSize: 11
            }

            Label { text: "Scope"; color: "#a89984"; font.pixelSize: 11 }
            Label {
                text: fileCount + " ROM" + (fileCount === 1 ? "" : "s")
                color: "#ebdbb2"
                font.pixelSize: 11
            }
        }

        Label {
            visible: matchController.unconfirmedMatchCount > 0 && !selectedOnly
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#fabd2f"
            font.pixelSize: 11
            text: matchController.unconfirmedMatchCount + " pending match(es) will be confirmed before bundling."
        }

        Label {
            id: statusLabel
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#83a598"
            font.pixelSize: 11
        }

        Label {
            visible: organizeController.lastError.length > 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#fb4934"
            font.pixelSize: 11
            text: organizeController.lastError
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: "Preview"
                enabled: libraryDest.length > 0 &&
                         fileCount > 0 &&
                         !exportController.exporting &&
                         !organizeController.organizing
                onClicked: {
                    organizeController.previewRenameOrganize(libraryDest, selectedOnly)
                    statusLabel.text = organizeController.progressMessage
                }
            }

            Button {
                text: "Undo last organize"
                flat: true
                enabled: !organizeController.organizing
                onClicked: organizeController.undoLast()
            }

            Item { Layout.fillWidth: true }

            Label {
                visible: exportController.exporting || organizeController.organizing
                text: exportController.exporting
                      ? exportController.progressMessage
                      : organizeController.progressMessage
                color: "#928374"
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.maximumWidth: 280
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#504945"
        }

        Label {
            text: "Preview (organize destinations)"
            font.bold: true
            font.pixelSize: 12
            color: "#a89984"
        }

        ListView {
            id: previewList
            Layout.fillWidth:  true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: organizeController.previewEntries

            delegate: Frame {
                required property var modelData
                width: previewList.width
                padding: 10

                background: Rectangle {
                    color: "#282828"
                    border.color: modelData.success ? "#504945" : "#cc241d"
                    radius: 6
                }

                ColumnLayout {
                    width: parent.width - 20
                    spacing: 2

                    Label {
                        text: modelData.success ? "Ready" : "Failed"
                        font.bold: true
                        font.pixelSize: 11
                        color: modelData.success ? "#b8bb26" : "#fb4934"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData.oldPath
                        font.pixelSize: 10
                        color: "#928374"
                        elide: Text.ElideMiddle
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData.newPath
                        font.pixelSize: 10
                        color: "#ebdbb2"
                        elide: Text.ElideMiddle
                    }
                    Label {
                        visible: modelData.error && modelData.error.length > 0
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: modelData.error
                        font.pixelSize: 10
                        color: "#fb4934"
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: previewList.count === 0
                text: "Click Preview to see destination paths."
                color: "#665c54"
                font.pixelSize: 12
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "Apply"
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            enabled: libraryDest.length > 0 &&
                     fileCount > 0 &&
                     !exportController.exporting &&
                     !organizeController.organizing
            onClicked: applyRenameOrganize()
        }
        Button {
            text: "Cancel"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: dialog.close()
        }

        function applyRenameOrganize() {
            if (!selectedOnly && matchController.unconfirmedMatchCount > 0)
                matchController.confirmAll()

            const scanDir = scanController.lastDirectory
            const template = organizeController.namingTemplate

            if (selectedOnly) {
                exportController.bundleSelected(scanDir, template)
                organizeController.applyOrganize(libraryDest)
            } else {
                exportController.bundleAll(scanDir, template)
                organizeController.applyOrganize(libraryDest)
            }

            statusLabel.text = exportController.lastMessage || organizeController.progressMessage
            if (organizeController.lastError.length === 0)
                dialog.close()
        }
    }
}
