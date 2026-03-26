import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import "components"

Page {
    id: patchPage
    title: "Patching"
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

    property var patchInfo: ({})
    property string selectedRom: ""
    property string selectedPatch: ""
    property string outputPath: ""
    property var recentPatches: []

    Component.onCompleted: {
        patchController.checkTools()
    }

    Connections {
        target: patchController

        function onPatchCompleted(path) {
            statusLabel.text = "Patch applied successfully: " + path
            statusLabel.color = theme.success
            addToRecent(selectedPatch, path)
        }

        function onPatchError(error) {
            statusLabel.text = "Patch failed: " + error
            statusLabel.color = theme.danger
        }

        function onCreatePatchCompleted(path) {
            statusLabel.text = "Patch created: " + path
            statusLabel.color = theme.success
        }

        function onCreatePatchError(error) {
            statusLabel.text = "Patch creation failed: " + error
            statusLabel.color = theme.danger
        }
    }

    function addToRecent(patch, output) {
        let item = {
            patch: patch,
            output: output,
            date: new Date().toLocaleDateString()
        }
        recentPatches.unshift(item)
        if (recentPatches.length > 10) {
            recentPatches.pop()
        }
        recentPatches = recentPatches.slice()  // Trigger update
    }

    FileDialog {
        id: romDialog
        title: "Select Base ROM"
        nameFilters: ["ROM files (*.nes *.sfc *.smc *.gb *.gbc *.gba *.md *.sms *.n64 *.z64 *.v64)", 
                      "All files (*)"]
        onAccepted: {
            selectedRom = selectedFile.toString().replace("file://", "")
        }
    }

    FileDialog {
        id: patchDialog
        title: "Select Patch File"
        nameFilters: ["Patch files (*.ips *.bps *.ups *.xdelta *.xdelta3 *.vcdiff)", 
                      "All files (*)"]
        onAccepted: {
            selectedPatch = selectedFile.toString().replace("file://", "")
            patchInfo = patchController.detectPatchFormat(selectedPatch)
            if (selectedRom) {
                outputPath = patchController.generateOutputPath(selectedRom, selectedPatch)
            }
        }
    }

    FileDialog {
        id: outputDialog
        title: "Save Patched ROM As"
        fileMode: FileDialog.SaveFile
        onAccepted: {
            outputPath = selectedFile.toString().replace("file://", "")
        }
    }

    header: ToolBar {
        background: Rectangle { color: theme.cardBg }
        RowLayout {
            anchors.fill: parent

            Label {
                text: "ROM Patching"
                font.pixelSize: 18
                font.bold: true
                color: theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            ThemedButton {
                text: "Apply Patch"
                isPrimary: true
                enabled: selectedRom && selectedPatch && !patchController.patching &&
                         patchInfo.supported
                onClicked: patchController.applyPatch(selectedRom, selectedPatch, outputPath)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        // Tool Status
        GroupBox {
            title: "Patching Tools"
            Layout.fillWidth: true

            GridLayout {
                columns: 4
                columnSpacing: 24
                rowSpacing: 8

                Label { text: "IPS Support:"; font.bold: true; color: theme.textPrimary }
                Label {
                    text: "Built-in"
                    color: theme.success
                }

                Label { text: "BPS/UPS Support:"; font.bold: true; color: theme.textPrimary }
                Label {
                    text: patchController.toolStatus.flips ? "Flips installed" : "Install flips for BPS/UPS"
                    color: patchController.toolStatus.flips ? theme.success : theme.warning
                }

                Label { text: "XDelta3 Support:"; font.bold: true; color: theme.textPrimary }
                Label {
                    text: patchController.toolStatus.xdelta3 ? "xdelta3 installed" : "Install xdelta3"
                    color: patchController.toolStatus.xdelta3 ? theme.success : theme.warning
                }

                Label { text: "Supported Formats:"; font.bold: true; color: theme.textPrimary }
                Label {
                    text: patchController.getSupportedFormats().join(", ")
                    color: theme.primary
                }
            }
        }

        // Patch Application Section
        GroupBox {
            title: "Apply Patch"
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

                // Base ROM selection
                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: "Base ROM:"
                        Layout.preferredWidth: 100
                        color: theme.textPrimary
                    }

                    ThemedTextField {
                        id: romField
                        Layout.fillWidth: true
                        text: selectedRom
                        readOnly: true
                        placeholderText: "Select the original ROM file..."
                    }

                    ThemedButton {
                        text: "Browse..."
                        onClicked: romDialog.open()
                    }
                }

                // Patch file selection
                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: "Patch File:"
                        Layout.preferredWidth: 100
                        color: theme.textPrimary
                    }

                    ThemedTextField {
                        id: patchField
                        Layout.fillWidth: true
                        text: selectedPatch
                        readOnly: true
                        placeholderText: "Select patch file (IPS, BPS, UPS, XDelta)..."
                    }

                    ThemedButton {
                        text: "Browse..."
                        onClicked: patchDialog.open()
                    }
                }

                // Patch info display
                Rectangle {
                    Layout.fillWidth: true
                    height: 60
                    color: theme.contentBg
                    radius: 4
                    visible: Object.keys(patchInfo).length > 0

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 24

                        Column {
                            Label { text: "Format"; color: theme.textSecondary; font.pixelSize: 11 }
                            Label { 
                                text: patchInfo.formatName || "Unknown"
                                font.bold: true
                                color: patchInfo.valid ? theme.textPrimary : theme.danger
                            }
                        }

                        Column {
                            Label { text: "Size"; color: theme.textSecondary; font.pixelSize: 11 }
                            Label { text: Math.round((patchInfo.size || 0) / 1024) + " KB" }
                        }

                        Column {
                            Label { text: "Status"; color: theme.textSecondary; font.pixelSize: 11 }
                            Label { 
                                text: patchInfo.supported ? "Supported" : 
                                      (patchInfo.valid ? "Tool Required" : patchInfo.error || "Invalid")
                                color: patchInfo.supported ? theme.success : theme.warning
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }

                // Output path
                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: "Output:"
                        Layout.preferredWidth: 100
                        color: theme.textPrimary
                    }

                    ThemedTextField {
                        id: outputField
                        Layout.fillWidth: true
                        text: outputPath
                        onTextChanged: outputPath = text
                        placeholderText: "Output path will be auto-generated..."
                    }

                    ThemedButton {
                        text: "Change..."
                        onClicked: outputDialog.open()
                    }
                }
            }
        }

        // Progress Section
        GroupBox {
            title: "Progress"
            Layout.fillWidth: true
            visible: patchController.patching

            ColumnLayout {
                anchors.fill: parent
                spacing: 8

                Label {
                    text: patchController.currentOperation
                    color: theme.textPrimary
                }

                ThemedProgressBar {
                    Layout.fillWidth: true
                    value: patchController.progress / 100
                    indeterminate: patchController.progress === 0
                }
            }
        }

        // Recent Patches
        GroupBox {
            title: "Recent Patches"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: recentPatches.length > 0

            ListView {
                id: recentList
                anchors.fill: parent
                model: recentPatches
                clip: true

                delegate: Rectangle {
                    width: recentList.width
                    height: 50
                    color: index % 2 === 0 ? theme.contentBg : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8

                        Column {
                            Layout.fillWidth: true
                            Label {
                                text: modelData.output.split("/").pop()
                                font.bold: true
                                color: theme.textPrimary
                            }
                            Label {
                                text: modelData.patch.split("/").pop()
                                color: theme.textSecondary
                                font.pixelSize: 12
                            }
                        }

                        Label {
                            text: modelData.date
                            color: theme.textSecondary
                            font.pixelSize: 12
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {}
            }
        }

        // Info when no recent patches
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: recentPatches.length === 0

            Column {
                anchors.centerIn: parent
                spacing: 8

                Label {
                    text: "No patches applied yet"
                    font.pixelSize: 16
                    color: theme.textSecondary
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Label {
                    text: "Select a base ROM and patch file to get started"
                    color: theme.textSecondary
                    anchors.horizontalCenter: parent.horizontalCenter
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
