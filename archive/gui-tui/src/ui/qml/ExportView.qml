import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import "components"

Page {
    id: exportPage
    title: "Export"
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

    property var preview: ({})
    property var availableSystems: []
    property var selectedSystems: []

    Component.onCompleted: {
        availableSystems = exportController.getAvailableSystems()
        updatePreview()
    }

    function updatePreview() {
        preview = exportController.getExportPreview(selectedSystems)
    }

    Connections {
        target: exportController
        function onExportCompleted(format, count, path) {
            statusLabel.text = format + " export completed: " + count + " items exported to " + path
            statusLabel.color = theme.success
        }
        function onExportFailed(format, error) {
            statusLabel.text = format + " export failed: " + error
            statusLabel.color = theme.danger
        }
    }

    FolderDialog {
        id: folderDialog
        title: "Select Export Directory"
        onAccepted: {
            var path = selectedFolder.toString().replace("file://", "")
            if (exportTypeCombo.currentIndex === 0) {
                exportController.exportToRetroArch(path, selectedSystems, false)
            } else if (exportTypeCombo.currentIndex === 1) {
                exportController.exportToEmulationStation(path, false)
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "Save Export File"
        fileMode: FileDialog.SaveFile
        onAccepted: {
            var path = selectedFile.toString().replace("file://", "")
            if (exportTypeCombo.currentIndex === 2) {
                exportController.exportToCSV(path, selectedSystems)
            } else if (exportTypeCombo.currentIndex === 3) {
                exportController.exportToJSON(path, true)
            }
        }
    }

    header: ToolBar {
        background: Rectangle { color: theme.cardBg }
        RowLayout {
            anchors.fill: parent

            Label {
                text: "Export Library"
                font.pixelSize: 18
                font.bold: true
                color: theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            ThemedComboBox {
                id: exportTypeCombo
                model: ["RetroArch Playlists", "EmulationStation Gamelists", "CSV Report", "JSON Export"]
                Layout.preferredWidth: 200
            }

            ThemedButton {
                text: "Export"
                icon.name: "document-export"
                enabled: !exportController.exporting && preview.totalGames > 0
                isPrimary: true
                onClicked: {
                    if (exportTypeCombo.currentIndex <= 1) {
                        folderDialog.open()
                    } else {
                        fileDialog.nameFilters = exportTypeCombo.currentIndex === 2 
                            ? ["CSV files (*.csv)"] 
                            : ["JSON files (*.json)"]
                        fileDialog.open()
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // Export type description
        Rectangle {
            Layout.fillWidth: true
            height: 80
            color: theme.cardBg
            radius: 8

            RowLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 20

                Image {
                    source: {
                        switch (exportTypeCombo.currentIndex) {
                            case 0: return "qrc:/icons/retroarch.png"
                            case 1: return "qrc:/icons/esde.png"
                            default: return ""
                        }
                    }
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    visible: source != ""
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5

                    Label {
                        text: {
                            switch (exportTypeCombo.currentIndex) {
                                case 0: return "RetroArch Playlist Export"
                                case 1: return "EmulationStation Gamelist Export"
                                case 2: return "CSV Spreadsheet Export"
                                case 3: return "JSON Data Export"
                            }
                        }
                        font.bold: true
                        color: theme.textPrimary
                    }

                    Label {
                        text: {
                            switch (exportTypeCombo.currentIndex) {
                                case 0: return "Creates .lpl playlist files compatible with RetroArch. Includes CRC32 hashes for game matching."
                                case 1: return "Creates gamelist.xml files for ES-DE frontends. One gamelist per system folder."
                                case 2: return "Exports library to a spreadsheet-compatible CSV file with all metadata."
                                case 3: return "Exports complete library data to JSON format for backup or processing."
                            }
                        }
                        color: theme.textSecondary
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // System filter and preview
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 20

            // System selection
            Rectangle {
                Layout.preferredWidth: 250
                Layout.fillHeight: true
                color: theme.cardBg
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: "Systems"
                            font.bold: true
                            color: theme.textPrimary
                        }

                        Item { Layout.fillWidth: true }

                        ThemedButton {
                            text: "All"
                            flat: true
                            font.pixelSize: 11
                            onClicked: {
                                selectedSystems = []
                                updatePreview()
                            }
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: availableSystems

                        delegate: ThemedCheckBox {
                            width: parent.width
                            text: modelData.name + " (" + modelData.gameCount + ")"
                            checked: selectedSystems.length === 0 || selectedSystems.includes(modelData.name)
                            onClicked: {
                                if (checked) {
                                    if (selectedSystems.length === 0) {
                                        // Switching from "all" to specific
                                        selectedSystems = [modelData.name]
                                    } else {
                                        selectedSystems = selectedSystems.concat([modelData.name])
                                    }
                                } else {
                                    selectedSystems = selectedSystems.filter(s => s !== modelData.name)
                                }
                                updatePreview()
                            }
                        }
                    }
                }
            }

            // Preview panel
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: theme.cardBg
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 15

                    Label {
                        text: "Export Preview"
                        font.bold: true
                        color: theme.textPrimary
                    }

                    // Summary cards
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15

                        Rectangle {
                            Layout.preferredWidth: 140
                            height: 80
                            color: theme.contentBg
                            radius: 6

                            ColumnLayout {
                                anchors.centerIn: parent

                                Label {
                                    text: preview.totalGames || 0
                                    font.pixelSize: 28
                                    font.bold: true
                                    color: theme.primary
                                    Layout.alignment: Qt.AlignHCenter
                                }

                                Label {
                                    text: "Games"
                                    color: theme.textSecondary
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 140
                            height: 80
                            color: theme.contentBg
                            radius: 6

                            ColumnLayout {
                                anchors.centerIn: parent

                                Label {
                                    text: preview.totalFiles || 0
                                    font.pixelSize: 28
                                    font.bold: true
                                    color: theme.info
                                    Layout.alignment: Qt.AlignHCenter
                                }

                                Label {
                                    text: "Files"
                                    color: theme.textSecondary
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 140
                            height: 80
                            color: theme.contentBg
                            radius: 6

                            ColumnLayout {
                                anchors.centerIn: parent

                                Label {
                                    text: (preview.systems || []).length
                                    font.pixelSize: 28
                                    font.bold: true
                                    color: theme.success
                                    Layout.alignment: Qt.AlignHCenter
                                }

                                Label {
                                    text: "Systems"
                                    color: theme.textSecondary
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }

                    // Per-system breakdown
                    Label {
                        text: "Per-System Breakdown"
                        font.bold: true
                        visible: (preview.systems || []).length > 0
                        color: theme.textPrimary
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: preview.systems || []

                        delegate: Rectangle {
                            width: parent.width
                            height: 35
                            color: index % 2 === 0 ? theme.contentBg : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 5
                                spacing: 10

                                Label {
                                    text: modelData.system
                                    font.bold: true
                                    Layout.preferredWidth: 150
                                    color: theme.textPrimary
                                }

                                Label {
                                    text: modelData.games + " games"
                                    Layout.fillWidth: true
                                    color: theme.textPrimary
                                }

                                Label {
                                    text: modelData.files + " files"
                                    color: theme.textSecondary
                                }
                            }
                        }
                    }
                }
            }
        }

        // Progress bar
        Rectangle {
            Layout.fillWidth: true
            height: 50
            color: theme.cardBg
            radius: 8
            visible: exportController.exporting

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 15

                Label {
                    text: "Exporting..."
                    color: theme.textPrimary
                }

                ThemedProgressBar {
                    Layout.fillWidth: true
                    from: 0
                    to: exportController.exportTotal
                    value: exportController.exportProgress
                }

                Label {
                    text: exportController.exportProgress + " / " + exportController.exportTotal
                    color: theme.textSecondary
                }

                ThemedButton {
                    text: "Cancel"
                    onClicked: exportController.cancelExport()
                }
            }
        }

        // Status label
        Label {
            id: statusLabel
            Layout.fillWidth: true
            color: theme.textSecondary
            wrapMode: Text.WordWrap
        }

        // Last export info
        RowLayout {
            Layout.fillWidth: true
            visible: exportController.lastExportPath.length > 0

            Label {
                text: "Last export:"
                color: theme.textSecondary
            }

            Label {
                text: exportController.lastExportPath
                color: theme.textMuted
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
        }
    }
}
