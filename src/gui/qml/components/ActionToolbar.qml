import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Remus.Gui

// TMM-style primary workflow actions for the library home screen.
RowLayout {
    id: root

    spacing: 4

    signal pipelineDrawerRequested()
    signal runAllRequested()
    signal settingsRequested()
    signal utilityToolRequested(int tabIndex)
    signal editRequested()
    signal matchEnrichRequested()
    signal renameOrganizeRequested()

    property bool pendingEnrich: false
    readonly property bool libraryReady: appController.libraryOpen

    function romSourceDirectory() {
        const fromSettings = settingsController.stringValue("gui/rom_source_directory", "")
        if (fromSettings.length > 0)
            return fromSettings
        return scanController.lastDirectory
    }

    function persistRomSourceDirectory(dir) {
        const cleaned = dir.trimmed()
        if (cleaned.length === 0)
            return
        settingsController.setValue("gui/rom_source_directory", cleaned)
        scanController.lastDirectory = cleaned
    }

    FolderDialog {
        id: scanFolderDialog
        title: "Select ROM source folder"
        onAccepted: {
            const dir = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""))
            root.persistRomSourceDirectory(dir)
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

    TmmToolButton {
        text: "Update library"
        iconName: "view-refresh-symbolic"
        enabled: libraryReady && !scanController.scanning
        onClicked: {
            const dir = root.romSourceDirectory()
            if (dir.length > 0)
                scanController.startScan(dir)
            else
                scanFolderDialog.open()
        }
    }

    TmmToolButton {
        text: "Match & enrich"
        iconName: "system-search-symbolic"
        enabled: libraryReady &&
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

    TmmToolButton {
        text: "Edit"
        iconName: "document-edit-symbolic"
        enabled: libraryReady && appController.selectedFileId > 0
        onClicked: root.editRequested()
    }

    TmmToolButton {
        text: "Rename & organize"
        iconName: "folder-download-symbolic"
        enabled: libraryReady &&
                 !exportController.exporting &&
                 !organizeController.organizing
        onClicked: root.renameOrganizeRequested()
    }

    ToolSeparator { Layout.topMargin: 4; Layout.bottomMargin: 4 }

    TmmToolButton {
        text: "Tools"
        iconName: "applications-tools-symbolic"
        enabled: true
        onClicked: toolsMenu.open()

        Menu {
            id: toolsMenu

            MenuItem {
                text: "Pipeline stages…"
                enabled: libraryReady
                onTriggered: root.pipelineDrawerRequested()
            }
            MenuItem {
                text: workflowController.running ? "Cancel run all" : "Run all stages…"
                enabled: libraryReady
                onTriggered: root.runAllRequested()
            }
            MenuSeparator {}
            MenuItem {
                text: "Import DAT…"
                enabled: libraryReady
                onTriggered: root.utilityToolRequested(0)
            }
            MenuItem {
                text: "Verify ROM…"
                enabled: libraryReady
                onTriggered: root.utilityToolRequested(1)
            }
            MenuItem {
                text: "Apply patch…"
                enabled: libraryReady
                onTriggered: root.utilityToolRequested(2)
            }
            MenuItem {
                text: "Mod catalog…"
                enabled: libraryReady
                onTriggered: root.utilityToolRequested(3)
            }
            MenuItem {
                text: "Export library…"
                enabled: libraryReady
                onTriggered: root.utilityToolRequested(4)
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
