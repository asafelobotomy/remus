import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Remus.Gui

// Advanced pipeline stages (opened from Tools → Pipeline stages).
Item {
    id: root

    anchors.fill: parent

    // ── Accordion state ──────────────────────────────────────────────────────
    // Which stage card is currently open (1–6).  0 = all collapsed.
    property int openStage: 1

    // Track the last file ID for which we auto-switched stage.
    // Auto-switching only happens when a different file is selected, not when
    // library state changes mid-work (e.g. after conversion completes).
    property int lastAutoStagedFileId: -1

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
        if (f["isOrganized"] || false)
            return 6  // Already organized — show outcome
        if (f["isBundled"] || false)
            return 6  // Bundled — next step is Organize
        if (!workflowController.artworkExistsForFile(appController.selectedFileId))
            return 3  // Artwork & Metadata
        if (!(f["isConverted"] || false))
            return 4  // Convert — needs conversion
        return 5      // Bundle & Rename
    }

    // Only auto-switch when a different file is selected.  Library data changes
    // (e.g. counts updating after conversion) must not hijack the open stage.
    // While Run All Stages is running, stage tracking is driven by the pipeline.
    onAutoStageChanged: {
        if (workflowController.running) return
        if (appController.selectedFileId <= 0) return
        if (appController.selectedFileId !== lastAutoStagedFileId) {
            lastAutoStagedFileId = appController.selectedFileId
            openStage = autoStage
        }
    }

    Connections {
        target: appController
        function onSelectedFileIdChanged() {
            if (workflowController.running) return
            lastAutoStagedFileId = appController.selectedFileId
            openStage = autoStage
        }
    }

    // Track Run All Stages pipeline: open the relevant stage card as the
    // pipeline advances, then collapse everything when done.
    Connections {
        target: workflowController
        function onActiveStageChanged() {
            if (workflowController.running)
                openStage = workflowController.activeStage
        }
        function onRunningChanged() {
            if (!workflowController.running)
                openStage = 0  // collapse all when pipeline finishes
        }
    }

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
        onAccepted: {
            destDirField.text = decodeURIComponent(
                selectedFolder.toString().replace(/^file:\/\//, ""))
            settingsController.setValue("gui/organize_destination", destDirField.text)
        }
    }

    // ── Apply: no organize directory warning ─────────────────────────────────
    Dialog {
        id: applyNoDirDialog
        title: "No Organize Directory"
        modal: true
        anchors.centerIn: Overlay.overlay

        Label {
            text: "Please select an Organize directory in section 6 before using Apply."
            wrapMode: Text.WordWrap
            width: 320
        }

        standardButtons: Dialog.Ok
    }

    // ── Apply: confirmation dialog ───────────────────────────────────────────
    Dialog {
        id: applyConfirmDialog
        title: "Apply All \u2014 Confirm"
        modal: true
        anchors.centerIn: Overlay.overlay

        ColumnLayout {
            width: 400
            spacing: 10

            Label {
                text: "The following actions will be applied to <b>all</b> matched ROMs:"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                textFormat: Text.RichText
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#504945"
            }

            Label {
                visible: matchController.unconfirmedMatchCount > 0
                text: "\u2022 Confirm " + matchController.unconfirmedMatchCount +
                      " pending Hash & Match result(s)"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#fabd2f"
            }

            Label {
                text: "\u2022 Bundle & Rename using template: <i>" +
                      bundleNameTemplate.editText + "</i>"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                textFormat: Text.RichText
            }

            Label {
                text: "\u2022 Organize ROMs into: <i>" +
                      destDirField.text + "/Remus Library</i>"
                visible: destDirField.text.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                textFormat: Text.RichText
            }

            Label {
                visible: destDirField.text.length === 0
                text: "\u2022 Organize: skipped (no destination directory set)"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#a89984"
            }

            Label {
                visible: scanController.lastDirectory.length > 0
                text: "Scan directory: " + scanController.lastDirectory
                font.pixelSize: 11
                color: "#928374"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        standardButtons: Dialog.Ok | Dialog.Cancel

        onAccepted: {
            matchController.confirmAll()
            exportController.bundleAll(scanController.lastDirectory,
                                       bundleNameTemplate.editText)
            organizeController.applyOrganize(destDirField.text + "/Remus Library")
        }
    }

    ScrollView {
        anchors.fill: parent
        contentWidth:  availableWidth
        clip:          true

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
                        onClicked: {
                            if (workflowController.running) {
                                workflowController.cancel()
                            } else {
                                workflowController.runAll(
                                    scanController.lastDirectory,
                                    destDirField.text.length > 0
                                        ? destDirField.text + "/Remus Library"
                                        : "",
                                    bundleNameTemplate.editText)
                            }
                        }
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
                            text:      "Hash && Match All"
                            enabled:   !hashController.hashing &&
                                       !matchController.matching &&
                                       appController.libraryOpen
                            onClicked: workflowController.hashAndMatchAll()
                        }
                        Button {
                            text:      "Hash && Match Selected"
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
                        visible:       hashController.hashing || matchController.matching
                    }

                    // ── Confirm All (global) ─────────────────────────────────
                    // Visible whenever there are unconfirmed matches, regardless
                    // of whether a specific file is selected.
                    RowLayout {
                        visible:          matchController.unconfirmedMatchCount > 0
                        Layout.fillWidth: true
                        spacing: 6

                        Button {
                            text:          "Confirm All (" + matchController.unconfirmedMatchCount + ")"
                            enabled:       !matchController.matching
                            font.pixelSize: 11
                            padding:        6
                            onClicked:      matchController.confirmAll()
                        }

                        Item { Layout.fillWidth: true }
                    }

                    // ── Confirm / Reject selected match ──────────────────────
                    // Only shown once the selected file has been hashed.
                    RowLayout {
                        visible:          (appController.selectedFileData.md5 || "").length > 0
                        Layout.fillWidth: true
                        spacing: 6

                        Label {
                            text:           appController.selectedMatchData.confirmed
                                            ? "Matched" : "Match Unconfirmed"
                            font.bold:      true
                            font.pixelSize: 12
                            color:          appController.selectedMatchData.confirmed
                                            ? "#b8bb26" : "#cc241d"
                        }

                        Button {
                            text:      "✓ Confirm Selected"
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
                        visible:          (appController.selectedFileData.md5 || "").length > 0 &&
                                          matchController.lastMessage.length > 0
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
                                           convertFormatCombo.currentText, "",
                                           scanController.lastDirectory)
                        }
                        Button {
                            text:      "Convert All"
                            enabled:   appController.libraryOpen &&
                                       !conversionController.converting
                            onClicked: conversionController.convertAll(
                                           convertFormatCombo.currentText, "",
                                           scanController.lastDirectory)
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

                // ── Stage 5: Bundle & Rename ─────────────────────────────────
                StageCard {
                    stageTitle: "5 · Bundle & Rename"
                    expanded:   openStage === 5
                    onToggleRequested: openStage = (openStage === 5 ? 0 : 5)

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Button {
                            text:      "Bundle Selected"
                            enabled:   appController.selectedFileId > 0 &&
                                       !exportController.exporting
                            onClicked: exportController.bundleSelected(scanController.lastDirectory, bundleNameTemplate.editText)
                        }
                        Button {
                            text:      "Bundle All"
                            enabled:   appController.libraryOpen &&
                                       !exportController.exporting
                            onClicked: exportController.bundleAll(scanController.lastDirectory, bundleNameTemplate.editText)
                        }
                        Item { Layout.fillWidth: true }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Label {
                            text:           "Name:"
                            color:          "#a89984"
                            font.pixelSize: 12
                        }
                        ComboBox {
                            id:             bundleNameTemplate
                            Layout.fillWidth: true
                            editable:       true
                            model:          ["{title} ({region})",
                                             "{title}",
                                             "{title} ({year})",
                                             "{title} ({system})",
                                             "{title} ({region}) [{system}]"]
                            currentIndex:   0
                            font.pixelSize: 12
                        }
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
                            text:             settingsController.stringValue("gui/organize_destination", "")
                            onEditingFinished: settingsController.setValue("gui/organize_destination", text)
                        }
                        Button {
                            text:      "Browse"
                            flat:      true
                            onClicked: organizeFolderDialog.open()
                        }
                        Button {
                            text:      "Organize All"
                            enabled:   appController.libraryOpen &&
                                       !organizeController.organizing &&
                                       destDirField.text.length > 0
                            onClicked: organizeController.organizeAll(
                                           destDirField.text + "/Remus Library")
                        }
                        Button {
                            text:      "Organize"
                            enabled:   appController.libraryOpen &&
                                       !organizeController.organizing &&
                                       destDirField.text.length > 0
                            onClicked: organizeController.applyOrganize(
                                           destDirField.text + "/Remus Library")
                        }
                        Button {
                            text:      "Undo"
                            flat:      true
                            onClicked: organizeController.undoLast()
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

                // ── Apply ────────────────────────────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Button {
                        id: applyButton
                        text: "Apply"
                        highlighted: true
                        enabled: appController.libraryOpen &&
                                 !hashController.hashing &&
                                 !matchController.matching &&
                                 !artworkController.downloading &&
                                 !exportController.exporting &&
                                 !organizeController.organizing &&
                                 (matchController.unconfirmedMatchCount +
                                  workflowController.enrichCount +
                                  workflowController.doneCount) > 0
                        onClicked: {
                            if (destDirField.text.length === 0)
                                applyNoDirDialog.open()
                            else
                                applyConfirmDialog.open()
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                Item {
                    Layout.preferredHeight: 10
                }
        }
    }
}
