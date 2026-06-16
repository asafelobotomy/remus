import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Remus.Gui

Item {
    id: root

    Layout.fillWidth: true
    Layout.fillHeight: true

    readonly property var exportFormats: [
        { label: "RetroArch playlist (.lpl)", value: "retroarch", pickFolder: false },
        { label: "EmulationStation gamelist.xml", value: "emustation", pickFolder: false },
        { label: "LaunchBox XML", value: "launchbox", pickFolder: false },
        { label: "CSV report", value: "csv", pickFolder: false },
        { label: "JSON export", value: "json", pickFolder: false },
        { label: "M3U playlists (multi-disc)", value: "m3u", pickFolder: true }
    ]

    property var preview: ({ totalGames: 0, systems: [] })

    function refreshPreview() {
        root.preview = exportController.exportPreview(systemsField.text)
    }

    Component.onCompleted: refreshPreview()

    Connections {
        target: appController
        function onLibraryOpened() { root.refreshPreview() }
        function onLibraryClosed() { root.refreshPreview() }
    }

    FolderDialog {
        id: folderDialog
        title: "Select export directory"
        onAccepted: {
            const dir = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//, ""))
            runExport(dir)
        }
    }

    FileDialog {
        id: fileDialog
        title: "Save export file"
        fileMode: FileDialog.SaveFile
        onAccepted: {
            const path = decodeURIComponent(selectedFile.toString().replace(/^file:\/\//, ""))
            runExport(path)
        }
    }

    function runExport(path) {
        const format = exportFormats[formatCombo.currentIndex].value
        if (format === "m3u") {
            exportController.generateM3uPlaylists(path, systemsField.text)
            return
        }
        exportController.exportFrontend(format, path, systemsField.text)
    }

    function openExportPicker() {
        const format = exportFormats[formatCombo.currentIndex].value
        if (format === "m3u") {
            folderDialog.open()
            return
        }
        if (format === "csv") {
            fileDialog.nameFilters = ["CSV files (*.csv)"]
            fileDialog.open()
        } else if (format === "json") {
            fileDialog.nameFilters = ["JSON files (*.json)"]
            fileDialog.open()
        } else if (format === "retroarch") {
            fileDialog.nameFilters = ["RetroArch playlist (*.lpl)"]
            fileDialog.open()
        } else {
            fileDialog.nameFilters = ["XML files (*.xml)"]
            fileDialog.open()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        Label {
            text: "Export Library"
            font.pixelSize: 26
            font.bold: true
            color: "#fbf1c7"
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#a89984"
            text: "Write matched library entries to RetroArch, EmulationStation, LaunchBox, CSV, JSON, or M3U playlists."
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label { text: "Format"; color: "#ebdbb2" }
            ComboBox {
                id: formatCombo
                Layout.fillWidth: true
                model: root.exportFormats.map(function(item) { return item.label })
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label { text: "Systems"; color: "#ebdbb2" }
            TextField {
                id: systemsField
                Layout.fillWidth: true
                placeholderText: "Leave empty for all systems, or comma-separated names"
                onEditingFinished: root.refreshPreview()
            }
            Button {
                text: "Preview"
                onClicked: root.refreshPreview()
            }
        }

        Frame {
            Layout.fillWidth: true
            padding: 12

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                Label {
                    text: "Preview: " + root.preview.totalGames + " matched files"
                    font.bold: true
                    color: "#fbf1c7"
                }

                Repeater {
                    model: root.preview.systems || []
                    delegate: Label {
                        required property var modelData
                        text: modelData.name + ": " + modelData.count
                        color: "#a89984"
                    }
                }
            }
        }

        ProgressCard {
            Layout.fillWidth: true
            title: "Export Progress"
            progressValue: exportController.exportProgress
            progressTotal: exportController.exportTotal
            message: exportController.progressMessage.length > 0
                     ? exportController.progressMessage
                     : exportController.lastMessage
            visible: exportController.exporting || exportController.lastMessage.length > 0
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                text: "Export…"
                enabled: appController.libraryOpen && !exportController.exporting && root.preview.totalGames > 0
                onClicked: openExportPicker()
            }
        }
    }
}
