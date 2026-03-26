import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import "components"

Page {
    id: verificationPage
    title: "Verification"
    background: Rectangle { color: theme.mainBg }
    palette.window: theme.mainBg
    palette.windowText: theme.textPrimary
    palette.base: theme.mainBg
    palette.text: theme.textPrimary
    palette.button: theme.mainBg
    palette.buttonText: theme.textPrimary
    palette.highlight: theme.primary
    palette.highlightedText: theme.textLight
    palette.placeholderText: theme.textMuted

    property var results: []
    property var summary: ({})
    property var importedDats: []

    Component.onCompleted: {
        refreshDatList()
    }

    function refreshDatList() {
        importedDats = verificationController.getImportedDats()
    }

    Connections {
        target: verificationController

        function onImportCompleted(count) {
            refreshDatList()
            statusLabel.text = "Imported " + count + " entries"
            statusLabel.color = theme.success
        }

        function onImportError(error) {
            statusLabel.text = "Import failed: " + error
            statusLabel.color = theme.danger
        }

        function onVerificationCompleted() {
            results = verificationController.results
            summary = verificationController.summary
            statusLabel.text = "Verification complete: " + summary.verified + " verified, " + 
                              summary.notInDat + " not in DAT"
            statusLabel.color = theme.textPrimary
        }

        function onVerificationError(error) {
            statusLabel.text = "Verification failed: " + error
            statusLabel.color = theme.danger
        }
    }

    FileDialog {
        id: datFileDialog
        title: "Select DAT File"
        nameFilters: ["DAT files (*.dat *.xml)", "All files (*)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file://", "")
            if (systemCombo.currentText) {
                verificationController.importDatFile(path, systemCombo.currentText)
            }
        }
    }

    FileDialog {
        id: exportDialog
        title: "Save Verification Report"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV files (*.csv)", "JSON files (*.json)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file://", "")
            var format = path.endsWith(".json") ? "json" : "csv"
            verificationController.exportResults(path, format)
        }
    }

    header: ToolBar {
        background: Rectangle { color: theme.cardBg }
        RowLayout {
            anchors.fill: parent

            Label {
                text: "ROM Verification"
                font.pixelSize: 18
                font.bold: true
                color: theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            ThemedButton {
                text: "Verify All"
                enabled: !verificationController.verifying && importedDats.length > 0
                isPrimary: true
                onClicked: verificationController.verifyAll()
            }

            ThemedButton {
                text: "Export Report"
                enabled: results.length > 0
                onClicked: exportDialog.open()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // DAT Management Section
        GroupBox {
            title: "DAT Files"
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

                // Import row
                RowLayout {
                    Layout.fillWidth: true

                    ThemedComboBox {
                        id: systemCombo
                        Layout.preferredWidth: 200
                        model: libraryController.getSystems().map(s => s.name)
                        displayText: currentText || "Select System"
                    }

                    ThemedButton {
                        text: "Import DAT"
                        onClicked: datFileDialog.open()
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: importedDats.length + " DAT file(s) imported"
                        color: theme.textSecondary
                    }
                }

                // Imported DATs list
                ListView {
                    id: datList
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, 150)
                    model: importedDats
                    clip: true

                    delegate: Rectangle {
                        width: datList.width
                        height: 50
                        color: index % 2 === 0 ? theme.contentBg : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8

                            Column {
                                Layout.fillWidth: true
                                Label {
                                    text: modelData.system
                                    font.bold: true
                                    color: theme.textPrimary
                                }
                                Label {
                                    text: modelData.name + " (v" + modelData.version + ")"
                                    color: theme.textSecondary
                                    font.pixelSize: 12
                                }
                            }

                            Label {
                                text: modelData.source
                                color: theme.primary
                                font.pixelSize: 12
                            }

                            ThemedButton {
                                text: "Remove"
                                flat: true
                                onClicked: {
                                    verificationController.removeDat(modelData.system)
                                    refreshDatList()
                                }
                            }
                        }
                    }
                }
            }
        }

        // Progress Section
        GroupBox {
            title: "Verification Progress"
            Layout.fillWidth: true
            visible: verificationController.verifying

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    text: verificationController.currentFile
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                    color: theme.textPrimary
                }

                ThemedProgressBar {
                    Layout.fillWidth: true
                    value: verificationController.total > 0 ? 
                           verificationController.progress / verificationController.total : 0
                }

                Label {
                    text: verificationController.progress + " / " + verificationController.total + " files"
                    color: theme.textSecondary
                }
            }
        }

        // Summary Section
        GroupBox {
            title: "Verification Summary"
            Layout.fillWidth: true
            visible: Object.keys(summary).length > 0

            GridLayout {
                columns: 4
                columnSpacing: 24
                rowSpacing: 8

                Label { text: "Total Files:"; font.bold: true; color: theme.textPrimary }
                Label { text: summary.totalFiles || 0; color: theme.textPrimary }

                Label { text: "Verified:"; font.bold: true; color: theme.textPrimary }
                Label { text: summary.verified || 0; color: theme.success }

                Label { text: "Not in DAT:"; font.bold: true; color: theme.textPrimary }
                Label { text: summary.notInDat || 0; color: theme.warning }

                Label { text: "No Hash:"; font.bold: true; color: theme.textPrimary }
                Label { text: summary.noHash || 0; color: theme.textSecondary }

                Label { text: "Mismatched:"; font.bold: true; color: theme.textPrimary }
                Label { text: summary.mismatched || 0; color: theme.danger }

                Label { text: "Corrupt:"; font.bold: true; color: theme.textPrimary }
                Label { text: summary.corrupt || 0; color: theme.danger }
            }
        }

        // Results Table
        GroupBox {
            title: "Verification Results"
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent

                // Filter row
                RowLayout {
                    Layout.fillWidth: true

                    ThemedComboBox {
                        id: filterCombo
                        model: ["All", "Verified", "Not in DAT", "No Hash", "Mismatch"]
                        Layout.preferredWidth: 150
                    }

                    ThemedTextField {
                        id: searchField
                        placeholderText: "Search..."
                        Layout.preferredWidth: 200
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: resultsList.count + " results"
                        color: theme.textSecondary
                    }
                }

                // Results list
                ListView {
                    id: resultsList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    model: {
                        let filtered = results
                        let filter = filterCombo.currentText.toLowerCase()
                        let search = searchField.text.toLowerCase()

                        if (filter !== "all") {
                            let statusMap = {
                                "verified": "verified",
                                "not in dat": "not_in_dat",
                                "no hash": "hash_missing",
                                "mismatch": "mismatch"
                            }
                            filtered = filtered.filter(r => r.status === statusMap[filter])
                        }

                        if (search) {
                            filtered = filtered.filter(r => 
                                r.filename.toLowerCase().includes(search) ||
                                (r.datName && r.datName.toLowerCase().includes(search))
                            )
                        }

                        return filtered
                    }

                    delegate: Rectangle {
                        width: resultsList.width
                        height: 60
                        color: index % 2 === 0 ? theme.contentBg : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 12

                            // Status icon
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: {
                                    switch (modelData.status) {
                                        case "verified": return theme.success
                                        case "not_in_dat": return theme.warning
                                        case "hash_missing": return theme.textSecondary
                                        case "mismatch": return theme.danger
                                        default: return theme.textPrimary
                                    }
                                }
                            }

                            Column {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: modelData.filename
                                    font.bold: true
                                    elide: Text.ElideMiddle
                                    width: parent.width
                                    color: theme.textPrimary
                                }

                                Label {
                                    text: modelData.datName || modelData.notes || ""
                                    color: theme.textSecondary
                                    font.pixelSize: 12
                                }
                            }

                            Label {
                                text: modelData.system
                                color: theme.primary
                            }

                            Label {
                                text: {
                                    switch (modelData.status) {
                                        case "verified": return "Verified"
                                        case "not_in_dat": return "Not in DAT"
                                        case "hash_missing": return "No Hash"
                                        case "mismatch": return "Mismatch"
                                        default: return "Unknown"
                                    }
                                }
                                color: {
                                    switch (modelData.status) {
                                        case "verified": return theme.success
                                        case "not_in_dat": return theme.warning
                                        case "hash_missing": return theme.textSecondary
                                        case "mismatch": return theme.danger
                                        default: return theme.textPrimary
                                    }
                                }
                                font.pixelSize: 12
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }
            }
        }

        // Status bar
        Label {
            id: statusLabel
            text: ""
            Layout.fillWidth: true
            color: theme.textSecondary
        }
    }
}
