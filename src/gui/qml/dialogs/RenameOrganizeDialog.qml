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
            color: Theme.textDim
            font.pixelSize: Theme.fontSm
            text: selectedOnly
                  ? "Selected ROM \u2014 bundle sidecars, then move into your organized library."
                  : "All confirmed matches \u2014 bundle sidecars, then move into your organized library."
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 4

            Label { text: "Naming template"; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                text: organizeController.namingTemplate
                color: Theme.textBody
                font.pixelSize: Theme.fontSm
            }

            Label { text: "Organize into"; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                text: libraryDest.length > 0 ? libraryDest : "Not configured \u2014 set in Settings"
                color: libraryDest.length > 0 ? Theme.textBody : Theme.error
                font.pixelSize: Theme.fontSm
            }

            Label { text: "Scope"; color: Theme.textMuted; font.pixelSize: Theme.fontSm }
            Label {
                text: fileCount + " ROM" + (fileCount === 1 ? "" : "s")
                color: Theme.textBody
                font.pixelSize: Theme.fontSm
            }
        }

        Label {
            visible: matchController.unconfirmedMatchCount > 0 && !selectedOnly
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.warn
            font.pixelSize: Theme.fontSm
            text: matchController.unconfirmedMatchCount + " pending match(es) will be confirmed before bundling."
        }

        Label {
            id: statusLabel
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.accentAlt
            font.pixelSize: Theme.fontSm
        }

        Label {
            visible: organizeController.lastError.length > 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.error
            font.pixelSize: Theme.fontSm
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
                color: Theme.textDim
                font.pixelSize: Theme.fontSm
                elide: Text.ElideRight
                Layout.maximumWidth: 280
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        Label {
            text: "Preview (organize destinations)"
            font.bold: true
            font.pixelSize: Theme.fontMd
            color: Theme.textMuted
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
                    color: Theme.surface
                    border.color: modelData.success ? Theme.border : Theme.error
                    radius: 6
                }

                ColumnLayout {
                    width: parent.width - 20
                    spacing: 2

                    Label {
                        text: modelData.success ? "Ready" : "Failed"
                        font.bold: true
                        font.pixelSize: Theme.fontSm
                        color: modelData.success ? Theme.success : Theme.error
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData.oldPath
                        font.pixelSize: Theme.fontXs
                        color: Theme.textDim
                        elide: Text.ElideMiddle
                    }
                    Label {
                        Layout.fillWidth: true
                        text: modelData.newPath
                        font.pixelSize: Theme.fontXs
                        color: Theme.textBody
                        elide: Text.ElideMiddle
                    }
                    Label {
                        visible: modelData.error && modelData.error.length > 0
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: modelData.error
                        font.pixelSize: Theme.fontXs
                        color: Theme.error
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: previewList.count === 0
                text: "Click Preview to see destination paths."
                color: Theme.textDisabled
                font.pixelSize: Theme.fontMd
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
