import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Remus.Gui

// The Workflow Workbench — three-panel layout:
//   Left:   QueueSidebar  (stage-bucketed file list)
//   Centre: Pipeline cards (6 stages, scrollable)
//   Right:  InspectorPanel (selected-file details)
Item {
    Layout.fillWidth:  true
    Layout.fillHeight: true

    // ── Accordion state ──────────────────────────────────────────────────────
    // Which stage card is currently open (1–6).  0 = all collapsed.
    property int openStage: 1

    // Derive the active stage from the selected ROM's processing state.
    // Reading enrichCount/doneCount ensures re-evaluation after artwork downloads.
    readonly property int autoStage: {
        const _e = workflowController.enrichCount  // dependency tracking
        const _d = workflowController.doneCount    // dependency tracking
        if (!appController.libraryOpen || appController.selectedFileId <= 0)
            return 1  // Scan
        const f = appController.selectedFileData
        const m = appController.selectedMatchData
        if (!f["md5"] || f["md5"] === "")
            return 2  // Hash & Match — needs hashing
        if (!m || !m["confirmed"])
            return 2  // Hash & Match — needs matching
        if (!workflowController.artworkExistsForFile(appController.selectedFileId))
            return 3  // Artwork & Metadata
        return 5      // Bundle
    }

    onAutoStageChanged: openStage = autoStage

    // ── Folder pickers ───────────────────────────────────────────────────────
    FolderDialog {
        id: scanFolderDialog
        title: "Select directory to scan"
        onAccepted: scanDirField.text = decodeURIComponent(
                        selectedFolder.toString().replace(/^file:\/\//, ""))
    }

    FolderDialog {
        id: organizeFolderDialog
        title: "Select destination directory"
        onAccepted: destDirField.text = decodeURIComponent(
                        selectedFolder.toString().replace(/^file:\/\//, ""))
    }

    SplitView {
        anchors.fill: parent
        orientation:  Qt.Horizontal

        // ── Left: Queue ──────────────────────────────────────────────────────
        QueueSidebar {
            SplitView.preferredWidth: 200
            SplitView.minimumWidth:   140
            SplitView.maximumWidth:   300
        }

        // ── Centre: Pipeline Cards ───────────────────────────────────────────
        ScrollView {
            SplitView.fillWidth:  true
            contentWidth:         availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            clip: true

            ColumnLayout {
                x:       8
                width:   Math.max(0, parent.width - 16)
                spacing: 10

                Item {
                    Layout.preferredHeight: 10
                }

                // Run-all toolbar
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        text:    workflowController.running ? "Cancel" : "▶ Run All Stages"
                        onClicked: workflowController.running
                                       ? workflowController.cancel()
                                       : workflowController.runAll()
                    }
                    Button {
                        text:     "↻ Refresh Counts"
                        flat:     true
                        onClicked: workflowController.refresh()
                    }
                    Item { Layout.fillWidth: true }
                }

                // ── Stage 1: Scan ────────────────────────────────────────────
                StageCard {
                    stageTitle: "1 · Scan"
                    expanded:   openStage === 1
                    onToggleRequested: openStage = (openStage === 1 ? 0 : 1)

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        TextField {
                            id:               scanDirField
                            Layout.fillWidth: true
                            placeholderText:  "Directory to scan…"
                            text:             scanController.lastDirectory
                            font.pixelSize:   12
                        }
                        Button {
                            text:      "Browse"
                            flat:      true
                            onClicked: scanFolderDialog.open()
                        }
                        Button {
                            text:      scanController.scanning ? "Stop" : "Scan"
                            enabled:   scanController.scanning || scanDirField.text.length > 0
                            onClicked: scanController.scanning
                                           ? scanController.stopScan()
                                           : scanController.startScan(scanDirField.text)
                        }
                    }

                    ProgressCard {
                        Layout.fillWidth: true
                        title:           "Scan Progress"
                        progressValue:   scanController.scannedFiles
                        progressTotal:   scanController.totalFiles
                        message:         scanController.progressMessage
                        visible:         scanController.scanning ||
                                         scanController.scannedFiles > 0
                    }
                }

                // ── Stage 2: Hash & Match ────────────────────────────────────
                StageCard {
                    stageTitle: "2 · Hash & Match"
                    stageCount: workflowController.identityCount
                    expanded:   openStage === 2
                    onToggleRequested: openStage = (openStage === 2 ? 0 : 2)

                    // Hash & Match buttons
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Button {
                            text:      "Hash & Match All"
                            enabled:   !hashController.hashing &&
                                       !matchController.matching &&
                                       appController.libraryOpen
                            onClicked: workflowController.hashAndMatchAll()
                        }
                        Button {
                            text:      "Hash & Match Selected"
                            enabled:   !hashController.hashing &&
                                       !matchController.matching &&
                                       appController.selectedFileId > 0
                            onClicked: workflowController.hashAndMatchSelected()
                        }
                        Item { Layout.fillWidth: true }
                    }

                    ProgressCard {
                        Layout.fillWidth: true
                        title:         "Hash & Match Progress"
                        progressValue: hashController.hashing
                                           ? hashController.hashedFiles
                                           : matchController.matchedFiles
                        progressTotal: hashController.hashing
                                           ? hashController.totalFiles
                                           : matchController.totalMatchFiles
                        message:       hashController.hashing
                                           ? hashController.progressMessage
                                           : matchController.progressMessage
                        visible:       hashController.hashing ||
                                       matchController.matching ||
                                       hashController.progressMessage.length > 0 ||
                                       matchController.progressMessage.length > 0
                    }

                    // ── Confirm / Reject selected match ──────────────────────
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Label {
                            text:           appController.selectedMatchData.confirmed
                                            ? "Matched" : "No match"
                            font.bold:      true
                            font.pixelSize: 12
                            color:          appController.selectedMatchData.confirmed
                                            ? "#b8bb26" : "#cc241d"
                        }

                        Button {
                            text:      "✓ Confirm"
                            enabled:   appController.selectedFileId > 0 &&
                                       !appController.selectedMatchData.confirmed
                            font.pixelSize: 11
                            padding:   6
                            onClicked: matchController.confirmSelected()
                        }
                        Button {
                            text:      "✗ Reject"
                            enabled:   appController.selectedFileId > 0 &&
                                       !appController.selectedMatchData.rejected
                            font.pixelSize: 11
                            padding:   6
                            onClicked: matchController.rejectSelected()
                        }

                        Item { Layout.fillWidth: true }
                    }

                    Label {
                        visible:          matchController.lastMessage.length > 0
                        Layout.fillWidth: true
                        text:             matchController.lastMessage
                        color:            "#83a598"
                        font.pixelSize:   10
                        wrapMode:         Text.WordWrap
                    }
                }

                // ── Stage 3: Artwork & Metadata ──────────────────────────────
                StageCard {
                    stageTitle: "3 · Artwork & Metadata"
                    stageCount: workflowController.enrichCount
                    expanded:   openStage === 3
                    onToggleRequested: openStage = (openStage === 3 ? 0 : 3)

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label { text: "Enrich:"; color: "#a89984"; font.pixelSize: 12 }
                        Button {
                            text:      "Enrich All"
                            enabled:   !artworkController.downloading && appController.libraryOpen
                            onClicked: artworkController.downloadAllMatched()
                        }
                        Button {
                            text:      "Enrich Selected"
                            enabled:   !artworkController.downloading &&
                                       appController.selectedFileId > 0
                            onClicked: artworkController.downloadSelected()
                        }
                    }

                    ProgressCard {
                        Layout.fillWidth: true
                        title:         "Enrich Progress"
                        progressValue: artworkController.downloadProgress
                        progressTotal: artworkController.downloadTotal
                        message:       artworkController.progressMessage
                        visible:       artworkController.downloading ||
                                       artworkController.progressMessage.length > 0
                    }
                }

                // ── Stage 4: Convert ─────────────────────────────────────────
                StageCard {
                    stageTitle: "4 · Convert"
                    expanded:   openStage === 4
                    onToggleRequested: openStage = (openStage === 4 ? 0 : 4)

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Label { text: "Format:"; color: "#a89984"; font.pixelSize: 12 }
                        ComboBox {
                            id:    convertFormatCombo
                            model: ["Auto", "CHD", "CSO", "RVZ", "WBFS", "PBP"]
                            font.pixelSize: 12
                        }
                        Button {
                            text:      "Convert Selected"
                            enabled:   appController.selectedFileId > 0 &&
                                       !conversionController.converting
                            onClicked: conversionController.convertSelected(
                                           convertFormatCombo.currentText, "")
                        }
                        Button {
                            text:      "Convert All"
                            enabled:   appController.libraryOpen &&
                                       !conversionController.converting
                            onClicked: conversionController.convertAll(
                                           convertFormatCombo.currentText, "")
                        }
                    }

                    ProgressCard {
                        Layout.fillWidth: true
                        title:         "Convert Progress"
                        progressValue: conversionController.progress
                        progressTotal: 100
                        message:       conversionController.progressMessage
                        visible:       conversionController.converting ||
                                       conversionController.progressMessage.length > 0
                    }
                }

                // ── Stage 5: Bundle ──────────────────────────────────────────
                StageCard {
                    stageTitle: "5 · Bundle"
                    expanded:   openStage === 5
                    onToggleRequested: openStage = (openStage === 5 ? 0 : 5)

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Button {
                            text:      "Bundle Selected"
                            enabled:   appController.selectedFileId > 0 &&
                                       !exportController.exporting
                            onClicked: exportController.bundleSelected("")
                        }
                        Button {
                            text:      "Bundle All"
                            enabled:   appController.libraryOpen &&
                                       !exportController.exporting
                            onClicked: exportController.bundleAll("")
                        }
                        Item { Layout.fillWidth: true }
                    }

                    ProgressCard {
                        Layout.fillWidth: true
                        title:         "Bundle Progress"
                        progressValue: exportController.bundledFiles
                        progressTotal: exportController.totalBundleFiles
                        message:       exportController.progressMessage
                        visible:       exportController.exporting ||
                                       exportController.progressMessage.length > 0
                    }
                }

                // ── Stage 6: Organize ────────────────────────────────────────
                StageCard {
                    stageTitle: "6 · Organize"
                    stageCount: workflowController.doneCount
                    expanded:   openStage === 6
                    onToggleRequested: openStage = (openStage === 6 ? 0 : 6)

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        TextField {
                            id:               destDirField
                            Layout.fillWidth: true
                            placeholderText:  "Destination directory…"
                            font.pixelSize:   12
                        }
                        Button {
                            text:      "Browse"
                            flat:      true
                            onClicked: organizeFolderDialog.open()
                        }
                        Button {
                            text:      "Preview"
                            enabled:   appController.libraryOpen
                            onClicked: organizeController.previewOrganize(
                                           destDirField.text + "/Remus Library")
                        }
                        Button {
                            text:      "Apply"
                            enabled:   appController.libraryOpen &&
                                       !organizeController.organizing
                            onClicked: organizeController.applyOrganize(
                                           destDirField.text + "/Remus Library")
                        }
                        Button {
                            text:      "Undo"
                            flat:      true
                            onClicked: organizeController.undoLast()
                        }
                    }

                    // Template field
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label { text: "Template:"; color: "#a89984"; font.pixelSize: 12 }
                        TextField {
                            Layout.fillWidth: true
                            text:             organizeController.namingTemplate
                            font.pixelSize:   12
                            onEditingFinished: organizeController.namingTemplate = text
                        }
                    }

                    // Preview list
                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(previewModel.count * 24, 120)
                        model:   organizeController.previewEntries
                        id:      previewModel
                        clip:    true
                        visible: count > 0
                        delegate: Label {
                            required property string modelData
                            width: ListView.view.width
                            text:  modelData
                            elide: Text.ElideLeft
                            font.pixelSize: 11
                            color: "#a89984"
                        }
                    }

                    ProgressCard {
                        Layout.fillWidth: true
                        title:         "Organize Progress"
                        progressValue: organizeController.organizedFiles
                        progressTotal: organizeController.organizing ? 0 : organizeController.totalOrganizeFiles
                        message:       organizeController.progressMessage
                        visible:       organizeController.organizing ||
                                       organizeController.progressMessage.length > 0
                    }

                    Label {
                        visible:   organizeController.lastError.length > 0
                        text:      organizeController.lastError
                        color:     "#fb4934"
                        wrapMode:  Text.WordWrap
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                }

                Item {
                    Layout.preferredHeight: 10
                }
            }
        }

        // ── Right: Inspector ─────────────────────────────────────────────────
        InspectorPanel {
            SplitView.preferredWidth: 280
            SplitView.minimumWidth:   200
            SplitView.maximumWidth:   400
        }
    }
}
