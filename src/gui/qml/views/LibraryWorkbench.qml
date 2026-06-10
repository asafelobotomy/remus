import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Remus.Gui

// Unified library home: filter sidebar + ROM table + inspector (TMM master–detail).
Item {
    Layout.fillWidth:  true
    Layout.fillHeight: true

    property alias pipelineDrawer: pipelineDrawer

    FolderDialog {
        id: runAllOrganizeDialog
        title: "Organize destination for run all"
        onAccepted: {
            const dest = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""))
            settingsController.setValue("gui/organize_destination", dest)
            runAllConfirmDialog.destDir = dest
            runAllConfirmDialog.open()
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
                text: "Run scan → hash/match → enrich → convert → bundle → organize on the library."
            }
            Label {
                visible: runAllConfirmDialog.destDir.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                textFormat: Text.RichText
                text: "Organize into: <i>" + runAllConfirmDialog.destDir + "/Remus Library</i>"
            }
        }

        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: {
            workflowController.runAll(
                scanController.lastDirectory,
                runAllConfirmDialog.destDir.length > 0
                    ? runAllConfirmDialog.destDir + "/Remus Library"
                    : "",
                organizeController.namingTemplate)
        }
    }

    Dialog {
        id: editMetadataDialog
        title: "Edit Metadata"
        modal: true
        width:  Math.min(560, Overlay.overlay.width * 0.9)
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
                    editMetadataDialog.close()
            }
            onRejected: {
                metadataEditorController.discard()
                editMetadataDialog.close()
            }
        }
    }

    MatchEnrichDialog {
        id: matchEnrichDialog
    }

    RenameOrganizeDialog {
        id: renameOrganizeDialog
    }

    SplitView {
        anchors.fill: parent
        orientation:  Qt.Horizontal

        QueueSidebar {
            SplitView.preferredWidth: 180
            SplitView.minimumWidth:   140
            SplitView.maximumWidth:   260
        }

        RomTable {
            SplitView.fillWidth: true
        }

        InspectorPanel {
            SplitView.preferredWidth: 280
            SplitView.minimumWidth:   200
            SplitView.maximumWidth:   400
            onMatchSearchRequested: matchEnrichDialog.open()
        }
    }

    Drawer {
        id: pipelineDrawer
        edge:       Qt.RightEdge
        width:      Math.min(parent.width * 0.55, 640)
        modal:      true
        interactive: true

        background: Rectangle {
            color: "#282828"
            border.color: "#504945"
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            ToolBar {
                Layout.fillWidth: true

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    Label {
                        text: "Pipeline Stages"
                        font.bold: true
                        color: "#fbf1c7"
                    }
                    Item { Layout.fillWidth: true }
                    ToolButton {
                        text: "Close"
                        onClicked: pipelineDrawer.close()
                    }
                }
            }

            PipelinePanel {
                Layout.fillWidth:  true
                Layout.fillHeight: true
            }
        }
    }

    function openPipelineDrawer() {
        pipelineDrawer.open()
    }

    function requestRunAll() {
        if (workflowController.running) {
            workflowController.cancel()
            return
        }
        const dest = settingsController.stringValue("gui/organize_destination", "")
        if (dest.length === 0)
            runAllOrganizeDialog.open()
        else {
            runAllConfirmDialog.destDir = dest
            runAllConfirmDialog.open()
        }
    }

    function openEditDialog() {
        editMetadataDialog.open()
    }

    function openMatchEnrichDialog() {
        matchEnrichDialog.open()
    }

    function openRenameOrganizeDialog() {
        renameOrganizeDialog.open()
    }
}
