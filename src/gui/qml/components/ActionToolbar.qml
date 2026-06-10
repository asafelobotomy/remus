import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Remus.Gui

// TMM-style primary workflow actions for the library home screen.
RowLayout {
    id: root

    spacing: 6

    signal pipelineDrawerRequested()
    signal runAllRequested()
    signal utilitiesRequested()
    signal settingsRequested()
    signal editRequested()

    property bool pendingEnrich: false
    property string organizeDestination: settingsController.stringValue("gui/organize_destination", "")

    FolderDialog {
        id: scanFolderDialog
        title: "Select directory to scan"
        onAccepted: {
            const dir = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""))
            scanController.lastDirectory = dir
            scanController.startScan(dir)
        }
    }

    FolderDialog {
        id: organizeFolderDialog
        title: "Select organize destination"
        onAccepted: {
            organizeDestination = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""))
            settingsController.setValue("gui/organize_destination", organizeDestination)
            renameOrganizeConfirmDialog.open()
        }
    }

    Dialog {
        id: renameOrganizeConfirmDialog
        title: "Rename & Organize"
        modal: true
        anchors.centerIn: Overlay.overlay

        ColumnLayout {
            width: 380
            spacing: 10

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: "Bundle matched ROMs and organize them into:"
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                textFormat: Text.RichText
                text: "<i>" + organizeDestination + "/Remus Library</i>"
                color: "#ebdbb2"
            }
            Label {
                visible: matchController.unconfirmedMatchCount > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#fabd2f"
                text: "Will confirm " + matchController.unconfirmedMatchCount + " pending match(es) first."
            }
        }

        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: {
            if (matchController.unconfirmedMatchCount > 0)
                matchController.confirmAll()
            exportController.bundleAll(scanController.lastDirectory, organizeController.namingTemplate)
            organizeController.applyOrganize(organizeDestination + "/Remus Library")
        }
    }

    Dialog {
        id: noOrganizeDirDialog
        title: "Organize Destination"
        modal: true
        anchors.centerIn: Overlay.overlay
        Label {
            text: "Choose a destination folder for organized ROMs."
            wrapMode: Text.WordWrap
            width: 320
        }
        standardButtons: Dialog.Ok
        onAccepted: organizeFolderDialog.open()
    }

    Connections {
        target: matchController
        function onMatchingChanged() {
            if (!root.pendingEnrich || matchController.matching)
                return
            root.pendingEnrich = false
            if (appController.selectedFileId > 0)
                artworkController.downloadSelected()
            else
                artworkController.downloadAllMatched()
        }
    }

    ToolButton {
        text: "Update library"
        enabled: appController.libraryOpen && !scanController.scanning
        onClicked: {
            if (scanController.lastDirectory.length > 0)
                scanController.startScan(scanController.lastDirectory)
            else
                scanFolderDialog.open()
        }
    }

    ToolButton {
        text: "Match & enrich"
        enabled: appController.libraryOpen &&
                 !hashController.hashing &&
                 !matchController.matching &&
                 !artworkController.downloading
        onClicked: {
            pendingEnrich = true
            if (appController.selectedFileId > 0)
                workflowController.hashAndMatchSelected()
            else
                workflowController.hashAndMatchAll()
        }
    }

    ToolButton {
        text: "Edit"
        enabled: appController.libraryOpen && appController.selectedFileId > 0
        onClicked: root.editRequested()
    }

    ToolButton {
        text: "Rename & organize"
        enabled: appController.libraryOpen &&
                 !exportController.exporting &&
                 !organizeController.organizing
        onClicked: {
            if (organizeDestination.length === 0)
                noOrganizeDirDialog.open()
            else
                renameOrganizeConfirmDialog.open()
        }
    }

    ToolSeparator {}

    ToolButton {
        text: "Tools ▼"
        onClicked: toolsMenu.open()

        Menu {
            id: toolsMenu

            MenuItem {
                text: "Pipeline stages…"
                onTriggered: root.pipelineDrawerRequested()
            }
            MenuItem {
                text: workflowController.running ? "Cancel run all" : "Run all stages…"
                onTriggered: root.runAllRequested()
            }
            MenuSeparator {}
            MenuItem {
                text: "Utilities"
                onTriggered: root.utilitiesRequested()
            }
            MenuItem {
                text: "Settings"
                onTriggered: root.settingsRequested()
            }
        }
    }

    Item { Layout.fillWidth: true }
}
