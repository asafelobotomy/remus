import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Remus.Gui

// Unified library home: filter sidebar + ROM table + inspector (TMM master–detail).
Item {
    Layout.fillWidth: true
    Layout.fillHeight: true

    readonly property string romSourceDirectory: {
        const fromSettings = settingsController.stringValue("gui/rom_source_directory", "");
        if (fromSettings.length > 0)
            return fromSettings;
        return scanController.lastDirectory;
    }

    FolderDialog {
        id: runAllOrganizeDialog
        title: "Organize destination for run all"
        onAccepted: {
            const dest = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""));
            settingsController.setValue("gui/organize_destination", dest);
            runAllConfirmDialog.destDir = dest;
            runAllConfirmDialog.open();
        }
    }

    Dialog {
        id: runAllConfirmDialog
        property string destDir: settingsController.stringValue("gui/organize_destination", "")

        title: "Run All Stages"
        modal: true
        anchors.centerIn: Overlay.overlay

        ColumnLayout {
            width: 360
            spacing: 8

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Run hash \u2192 match \u2192 confirm \u2192 artwork \u2192 convert \u2192 bundle \u2192 organize on the library."
            }
            Label {
                visible: runAllConfirmDialog.destDir.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                textFormat: Text.PlainText
                text: "Organize into: " + runAllConfirmDialog.destDir + "/Remus Library"
            }
        }

        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: {
            workflowController.runAll(romSourceDirectory, runAllConfirmDialog.destDir.length > 0 ? runAllConfirmDialog.destDir + "/Remus Library" : "", organizeController.namingTemplate);
        }
    }

    Dialog {
        id: editMetadataDialog
        title: "Edit Metadata"
        modal: true
        width: Math.min(560, Overlay.overlay.width * 0.9)
        height: Math.min(480, Overlay.overlay.height * 0.85)
        anchors.centerIn: Overlay.overlay

        onOpened: metadataEditorController.loadForSelectedFile()

        contentItem: MetadataEditor {
            anchors.fill: parent
        }

        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Save | DialogButtonBox.Cancel
            onAccepted: {
                if (metadataEditorController.save())
                    editMetadataDialog.close();
            }
            onRejected: {
                metadataEditorController.discard();
                editMetadataDialog.close();
            }
        }
    }

    MatchEnrichDialog {
        id: matchEnrichDialog
    }

    RenameOrganizeDialog {
        id: renameOrganizeDialog
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Frame {
            Layout.fillWidth: true
            visible: appController.libraryOpen
            padding: 10

            background: Rectangle {
                radius: 10
                color: Theme.panelBg
                border.color: Theme.panelBorder
            }

            RowLayout {
                width: parent.width
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: "ROM source folder"
                        font.pixelSize: Theme.fontXs
                        font.bold: true
                        color: Theme.textDim
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontMd
                        color: romSourceDirectory.length > 0 ? Theme.textBody : Theme.warn
                        text: romSourceDirectory.length > 0 ? romSourceDirectory : "Not set \u2014 choose a folder in Settings or use Update library"
                    }
                }

                ToolSeparator {}

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: "Library database"
                        font.pixelSize: Theme.fontXs
                        font.bold: true
                        color: Theme.textDim
                    }
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                        font.pixelSize: Theme.fontMd
                        color: Theme.textBody
                        text: appController.libraryPath
                    }
                }

                ColumnLayout {
                    spacing: 2

                    Label {
                        text: "In view"
                        font.pixelSize: Theme.fontXs
                        font.bold: true
                        color: Theme.textDim
                    }
                    Label {
                        font.pixelSize: Theme.fontMd
                        color: Theme.textBody
                        text: workflowController.queueFiles.length + " titles"
                    }
                }
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            QueueSidebar {
                SplitView.preferredWidth: 180
                SplitView.minimumWidth: 140
                SplitView.maximumWidth: 260
            }

            RomTable {
                SplitView.fillWidth: true
            }

            InspectorPanel {
                SplitView.preferredWidth: 280
                SplitView.minimumWidth: 200
                SplitView.maximumWidth: 400
                onMatchSearchRequested: matchEnrichDialog.open()
            }
        }
    }

    function requestRunAll() {
        if (workflowController.running) {
            workflowController.cancel();
            return;
        }
        const dest = settingsController.stringValue("gui/organize_destination", "");
        if (dest.length === 0)
            runAllOrganizeDialog.open();
        else {
            runAllConfirmDialog.destDir = dest;
            runAllConfirmDialog.open();
        }
    }

    function openEditDialog() {
        editMetadataDialog.open();
    }

    function openMatchEnrichDialog() {
        matchEnrichDialog.open();
    }

    function openRenameOrganizeDialog() {
        renameOrganizeDialog.open();
    }
}
