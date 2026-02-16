import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import "components"

Page {
    title: "CHD Conversions"
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
    
    header: ToolBar {
        background: Rectangle { color: theme.cardBg }
        RowLayout {
            anchors.fill: parent
            
            Label {
                text: "CHD Conversion & Archive Tools"
                font.pixelSize: 18
                font.bold: true
                color: theme.textPrimary
            }
            
            Item { Layout.fillWidth: true }
        }
    }
    
    ScrollView {
        anchors.fill: parent
        anchors.margins: 20
        
        ColumnLayout {
            width: parent.width - 40
            spacing: 20
            
            // CHD Conversion Section
            GroupBox {
                title: "Convert to CHD"
                Layout.fillWidth: true
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    
                    Label {
                        text: "Convert disc images (CUE/BIN, ISO) to compressed CHD format"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        color: theme.textSecondary
                    }
                    
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        
                        ThemedTextField {
                            id: chdInputFile
                            Layout.fillWidth: true
                            placeholderText: "Select disc image file..."
                            readOnly: true
                        }
                        
                        ThemedButton {
                            text: "Browse"
                            onClicked: chdFileDialog.open()
                        }
                    }
                    
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        
                        Label {
                            text: "Codec:"
                            color: theme.textPrimary
                        }
                        
                        ThemedComboBox {
                            id: codecCombo
                            model: ["Auto", "LZMA", "ZLIB", "FLAC", "Huffman"]
                            Layout.preferredWidth: 150
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        Label {
                            text: "Estimated savings: ~40-60%"
                            color: theme.success
                            font.bold: true
                            visible: chdInputFile.text !== ""
                        }
                    }
                    
                    ThemedButton {
                        text: "Convert to CHD"
                        icon.name: "document-save"
                        enabled: chdInputFile.text !== "" && !conversionController.converting
                        Layout.alignment: Qt.AlignRight
                        isPrimary: true
                        onClicked: {
                            conversionController.convertToCHD(
                                chdInputFile.text,
                                codecCombo.currentText.toLowerCase()
                            )
                        }
                    }
                }
            }
            
            // CHD Extraction Section
            GroupBox {
                title: "Extract CHD"
                Layout.fillWidth: true
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    
                    Label {
                        text: "Extract CHD files back to CUE/BIN format"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        color: theme.textSecondary
                    }
                    
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        
                        ThemedTextField {
                            id: chdExtractFile
                            Layout.fillWidth: true
                            placeholderText: "Select CHD file..."
                            readOnly: true
                        }
                        
                        ThemedButton {
                            text: "Browse"
                            onClicked: chdExtractDialog.open()
                        }
                    }
                    
                    ThemedButton {
                        text: "Extract CHD"
                        icon.name: "archive-extract"
                        enabled: chdExtractFile.text !== "" && !conversionController.converting
                        Layout.alignment: Qt.AlignRight
                        isPrimary: true
                        onClicked: conversionController.extractCHD(chdExtractFile.text)
                    }
                }
            }
            
            // Archive Extraction Section
            GroupBox {
                title: "Extract Archive"
                Layout.fillWidth: true
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    
                    Label {
                        text: "Extract ZIP, 7z, or RAR archives"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        color: theme.textSecondary
                    }
                    
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        
                        ThemedTextField {
                            id: archiveFile
                            Layout.fillWidth: true
                            placeholderText: "Select archive file..."
                            readOnly: true
                        }
                        
                        ThemedButton {
                            text: "Browse"
                            onClicked: archiveDialog.open()
                        }
                    }
                    
                    ThemedButton {
                        text: "Extract Archive"
                        icon.name: "archive-extract"
                        enabled: archiveFile.text !== "" && !conversionController.converting
                        Layout.alignment: Qt.AlignRight
                        isPrimary: true
                        onClicked: conversionController.extractArchive(archiveFile.text)
                    }
                }
            }
            
            // Progress Section
            GroupBox {
                title: "Progress"
                Layout.fillWidth: true
                visible: conversionController.converting
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    
                    ThemedProgressBar {
                        id: conversionProgress
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: 0
                        
                        Connections {
                            target: conversionController
                            function onConversionProgress(percent) {
                                conversionProgress.value = percent
                            }
                        }
                    }
                    
                    Label {
                        text: "Converting... Please wait"
                        color: theme.primary
                        font.bold: true
                    }
                }
            }
            
            Item { Layout.fillHeight: true }
        }
    }
    
    FileDialog {
        id: chdFileDialog
        title: "Select Disc Image"
        nameFilters: ["Disc Images (*.cue *.bin *.iso)"]
        onAccepted: {
            chdInputFile.text = selectedFile.toString().replace("file://", "")
        }
    }
    
    FileDialog {
        id: chdExtractDialog
        title: "Select CHD File"
        nameFilters: ["CHD Files (*.chd)"]
        onAccepted: {
            chdExtractFile.text = selectedFile.toString().replace("file://", "")
        }
    }
    
    FileDialog {
        id: archiveDialog
        title: "Select Archive"
        nameFilters: ["Archives (*.zip *.7z *.rar)"]
        onAccepted: {
            archiveFile.text = selectedFile.toString().replace("file://", "")
        }
    }
}
