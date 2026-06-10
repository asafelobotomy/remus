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
    signal settingsRequested()
    signal utilityToolRequested(int tabIndex)
    signal editRequested()
    signal matchEnrichRequested()
    signal renameOrganizeRequested()

    property bool pendingEnrich: false

    FolderDialog {
        id: scanFolderDialog
        title: "Select directory to scan"
        onAccepted: {
            const dir = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""))
            scanController.lastDirectory = dir
            scanController.startScan(dir)
        }
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
            if (appController.selectedFileId > 0) {
                root.matchEnrichRequested()
            } else {
                pendingEnrich = true
                workflowController.hashAndMatchAll()
            }
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
        onClicked: root.renameOrganizeRequested()
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
                text: "Import DAT…"
                onTriggered: root.utilityToolRequested(0)
            }
            MenuItem {
                text: "Verify ROM…"
                onTriggered: root.utilityToolRequested(1)
            }
            MenuItem {
                text: "Apply patch…"
                onTriggered: root.utilityToolRequested(2)
            }
            MenuItem {
                text: "Mod catalog…"
                onTriggered: root.utilityToolRequested(3)
            }
            MenuSeparator {}
            MenuItem {
                text: "Settings"
                onTriggered: root.settingsRequested()
            }
        }
    }

    Item { Layout.fillWidth: true }
}
