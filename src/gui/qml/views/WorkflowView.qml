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
                        message:         scanController.recentLogs.length > 0
                                           ? scanController.recentLogs[scanController.recentLogs.length - 1]
                                           : ""
                        visible:         scanController.scanning ||
                                         scanController.scannedFiles > 0
                    }
                }

                // ── Stage 2: Hash & Match ────────────────────────────────────
                StageCard {
                    stageTitle: "2 · Hash & Match"
                    stageCount: workflowController.identityCount

                    // Hash sub-section
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label { text: "Hash:"; color: "#a89984"; font.pixelSize: 12 }
                        Button {
                            text:      "All"
                            enabled:   !hashController.hashing && appController.libraryOpen
                            onClicked: hashController.startHashAll()
                        }
                        Button {
                            text:      "Selected"
                            enabled:   !hashController.hashing &&
                                       appController.selectedFileId > 0
                            onClicked: hashController.hashSelected()
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            from:    0
                            to:      hashController.totalFiles || 1
                            value:   hashController.hashedFiles
                            visible: hashController.hashing
                        }
                    }

                    // Match sub-section
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label { text: "Match:"; color: "#a89984"; font.pixelSize: 12 }
                        Button {
                            text:      "All"
                            enabled:   !matchController.matching && appController.libraryOpen
                            onClicked: matchController.matchAll()
                        }
                        Button {
                            text:      "Selected"
                            enabled:   !matchController.matching &&
                                       appController.selectedFileId > 0
                            onClicked: matchController.matchSelected()
                        }
                        Label {
                            text:    matchController.currentProvider
                            color:   "#a89984"
                            elide:   Text.ElideRight
                            Layout.fillWidth: true
                            font.pixelSize: 11
                        }
                    }

                    Label {
                        visible:   matchController.lastMessage.length > 0
                        text:      matchController.lastMessage
                        color:     "#a89984"
                        wrapMode:  Text.WordWrap
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                }

                // ── Stage 3: Artwork & Metadata ──────────────────────────────
                StageCard {
                    stageTitle: "3 · Artwork & Metadata"
                    stageCount: workflowController.enrichCount

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
                        ProgressBar {
                            Layout.fillWidth: true
                            from:    0
                            to:      artworkController.downloadTotal || 1
                            value:   artworkController.downloadProgress
                            visible: artworkController.downloading
                        }
                    }

                    Label {
                        visible:   artworkController.lastError.length > 0
                        text:      artworkController.lastError
                        color:     "#fb4934"
                        wrapMode:  Text.WordWrap
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                }

                // ── Stage 4: Convert ─────────────────────────────────────────
                StageCard {
                    stageTitle: "4 · Convert"

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

                    ProgressBar {
                        Layout.fillWidth: true
                        from:    0
                        to:      100
                        value:   conversionController.progress
                        visible: conversionController.converting
                    }

                    Label {
                        visible:   conversionController.lastMessage.length > 0
                        text:      conversionController.lastMessage
                        color:     "#a89984"
                        wrapMode:  Text.WordWrap
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                }

                // ── Stage 5: Bundle ──────────────────────────────────────────
                StageCard {
                    stageTitle: "5 · Bundle"

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

                    Label {
                        visible:   exportController.lastMessage.length > 0
                        text:      exportController.lastMessage
                        color:     "#a89984"
                        wrapMode:  Text.WordWrap
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                }

                // ── Stage 6: Organize ────────────────────────────────────────
                StageCard {
                    stageTitle: "6 · Organize"
                    stageCount: workflowController.doneCount

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
