import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "components"

Page {
    title: "Settings"
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
    
    Component.onCompleted: loadSettings()
    
    header: ToolBar {
        background: Rectangle { color: theme.cardBg }
        RowLayout {
            anchors.fill: parent
            
            Label {
                text: "Application Settings"
                font.pixelSize: 18
                font.bold: true
                color: theme.textPrimary
            }
            
            Item { Layout.fillWidth: true }
            
            ThemedButton {
                text: "Save Settings"
                icon.name: "document-save"
                isPrimary: true
                onClicked: saveSettings()
            }
        }
    }
    
    ScrollView {
        anchors.fill: parent
        anchors.margins: 20
        
        ColumnLayout {
            width: parent.width - 40
            spacing: 20
            
            // ===== Theme Settings =====
            GroupBox {
                title: "Appearance"
                Layout.fillWidth: true
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 15
                    
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15
                        
                        Label {
                            text: "Theme Mode:"
                            font.bold: true
                            color: theme.textPrimary
                        }
                        
                        ThemedComboBox {
                            id: themeMode
                            model: ["🌙 Dark Mode", "☀️ Light Mode"]
                            Layout.preferredWidth: 180
                            currentIndex: theme.isDarkMode ? 0 : 1
                        }
                        
                        Item { Layout.fillWidth: true }
                    }
                    
                    Label {
                        text: "Color scheme: Gruvbox - A retro inspired color palette"
                        color: theme.textSecondary
                        font.pixelSize: 11
                        font.bold: true
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: theme.border
                    }
                }
            }
            
            GroupBox {
                title: "Metadata Providers"
                Layout.fillWidth: true
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 15
                    
                    Label {
                        text: "ScreenScraper Credentials"
                        font.bold: true
                        color: theme.textPrimary
                    }
                    
                    Label {
                        text: "Required for metadata fetching from ScreenScraper"
                        color: theme.textSecondary
                        font.pixelSize: 11
                        font.bold: true
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    
                    GridLayout {
                        columns: 2
                        Layout.fillWidth: true
                        columnSpacing: 10
                        rowSpacing: 10
                        
                        Label { text: "Username:"; font.bold: true; color: theme.textPrimary }
                        ThemedTextField {
                            id: ssUsername
                            Layout.fillWidth: true
                            placeholderText: "ScreenScraper username"
                        }
                        
                        Label { text: "Password:"; font.bold: true; color: theme.textPrimary }
                        ThemedTextField {
                            id: ssPassword
                            Layout.fillWidth: true
                            placeholderText: "ScreenScraper password"
                            echoMode: TextInput.Password
                        }
                        
                        Label { text: "Dev ID:"; font.bold: true; color: theme.textPrimary }
                        ThemedTextField {
                            id: ssDevId
                            Layout.fillWidth: true
                            placeholderText: "Optional developer ID"
                        }
                        
                        Label { text: "Dev Password:"; font.bold: true; color: theme.textPrimary }
                        ThemedTextField {
                            id: ssDevPassword
                            Layout.fillWidth: true
                            placeholderText: "Optional developer password"
                            echoMode: TextInput.Password
                        }
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: theme.border
                    }
                    
                    Label {
                        text: "Provider Priority"
                        font.bold: true
                        color: theme.textPrimary
                    }
                    
                    ThemedComboBox {
                        id: providerPriority
                        Layout.fillWidth: true
                        model: ["ScreenScraper (Primary)", "TheGamesDB (Primary)", "IGDB (Primary)", "Auto"]
                    }
                }
            }
            
            GroupBox {
                title: "Organization"
                Layout.fillWidth: true
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 15
                    
                    Label {
                        text: "Naming Template"
                        font.bold: true
                        color: theme.textPrimary
                    }
                    
                    Label {
                        text: settingsController.defaults.templateVariableHint
                        color: theme.textSecondary
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    
                    ThemedTextField {
                        id: namingTemplate
                        Layout.fillWidth: true
                        text: settingsController.defaults.namingTemplate
                        placeholderText: settingsController.defaults.namingTemplate
                    }
                    
                    Label {
                        text: "Preview: " + previewTemplate()
                        color: theme.primary
                        font.italic: true
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: theme.border
                    }
                    
                    ThemedCheckBox {
                        id: organizeBySystem
                        text: "Organize files into system folders"
                        checked: true
                    }
                    
                    ThemedCheckBox {
                        id: preserveOriginals
                        text: "Keep backup of original files"
                        checked: false
                    }
                }
            }
            
            // ===== Local DAT Databases =====
            GroupBox {
                title: "Local DAT Databases"
                Layout.fillWidth: true
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 15
                    
                    Label {
                        text: "Offline ROM Identification"
                        font.bold: true
                        color: theme.textPrimary
                    }
                    
                    Label {
                        text: "Local DAT files (No-Intro, Redump) provide instant ROM identification without internet. DATs are automatically loaded from data/databases/."
                        color: theme.textSecondary
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                    
                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: theme.border
                    }
                    
                    // DAT file list
                    Repeater {
                        model: datManager.loadedDats
                        
                        delegate: Rectangle {
                            Layout.fillWidth: true
                            height: 80
                            color: theme.surface
                            border.color: theme.border
                            border.width: 1
                            radius: 4
                            
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 4
                                
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10
                                    
                                    Label {
                                        text: modelData.name
                                        font.bold: true
                                        font.pixelSize: 13
                                        color: theme.textPrimary
                                        Layout.fillWidth: true
                                    }
                                    
                                    Label {
                                        text: modelData.entryCount + " entries"
                                        font.pixelSize: 11
                                        color: theme.accent
                                    }
                                }
                                
                                Label {
                                    text: "Version: " + modelData.version + " • Loaded: " + modelData.loadedAt
                                    font.pixelSize: 10
                                    color: theme.textSecondary
                                    Layout.fillWidth: true
                                }
                                
                                Label {
                                    text: modelData.filePath
                                    font.pixelSize: 9
                                    color: theme.textTertiary
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                    
                    // Show message if no DATs loaded
                    Label {
                        visible: datManager.loadedDats.length === 0
                        text: "No DAT files loaded. Place .dat files in data/databases/ and restart Remus."
                        color: theme.textSecondary
                        font.pixelSize: 11
                        font.italic: true
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }
            }
            
            GroupBox {
                title: "Database"
                Layout.fillWidth: true
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    
                    Label {
                        text: "Database Location"
                        font.bold: true
                        color: theme.textPrimary
                    }
                    
                    Label {
                        text: "~/.local/share/Remus/Remus/remus.db"
                        color: theme.textSecondary
                        font.family: "monospace"
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }
                }
            }
            
            GroupBox {
                title: "Performance"
                Layout.fillWidth: true
                
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 15
                    
                    RowLayout {
                        Layout.fillWidth: true
                        
                        Label {
                            text: "Hash Algorithm:"
                            color: theme.textPrimary
                        }
                        
                        ThemedComboBox {
                            id: hashAlgorithm
                            Layout.preferredWidth: 150
                            model: ["Auto (System Default)", "CRC32", "MD5", "SHA1"]
                        }
                    }
                    
                    ThemedCheckBox {
                        id: parallelHashing
                        text: "Enable parallel hashing (faster but uses more CPU)"
                        checked: true
                    }
                }
            }
            
            Item { Layout.fillHeight: true }
        }
    }
    
    function loadSettings() {
        // Load theme mode and apply immediately
        var isDarkMode = (settingsController.getSetting("theme/darkMode", "true") !== "false");
        themeMode.currentIndex = isDarkMode ? 0 : 1;
        theme.setDarkMode(isDarkMode);
        
        ssUsername.text = settingsController.getSetting(settingsController.keys.screenscraperUsername, "");
        ssPassword.text = settingsController.getSetting(settingsController.keys.screenscraperPassword, "");
        ssDevId.text = settingsController.getSetting(settingsController.keys.screenscraperDevId, "");
        ssDevPassword.text = settingsController.getSetting(settingsController.keys.screenscraperDevPassword, "");
        
        var priority = settingsController.getSetting(
            settingsController.keys.metadataProviderPriority,
            settingsController.defaults.providerPriority
        );
        providerPriority.currentIndex = providerPriority.find(priority);
        
        namingTemplate.text = settingsController.getSetting(
            settingsController.keys.organizeNamingTemplate,
            settingsController.defaults.namingTemplate
        );
        organizeBySystem.checked = settingsController.getSetting(
            settingsController.keys.organizeBySystem,
            settingsController.defaults.organizeBySystem
        ) !== "false";
        preserveOriginals.checked = settingsController.getSetting(
            settingsController.keys.organizePreserveOriginals,
            settingsController.defaults.preserveOriginals
        ) === "true";
        
        var hashAlg = settingsController.getSetting(
            settingsController.keys.performanceHashAlgorithm,
            settingsController.defaults.hashAlgorithm
        );
        hashAlgorithm.currentIndex = hashAlgorithm.find(hashAlg);
        parallelHashing.checked = settingsController.getSetting(
            settingsController.keys.performanceParallelHashing,
            settingsController.defaults.parallelHashing
        ) !== "false";
    }
    
    function saveSettings() {
        // Apply theme change
        var isDarkMode = (themeMode.currentIndex === 0);
        theme.setDarkMode(isDarkMode);
        settingsController.setSetting("theme/darkMode", isDarkMode.toString());
        
        settingsController.setSetting(settingsController.keys.screenscraperUsername, ssUsername.text);
        settingsController.setSetting(settingsController.keys.screenscraperPassword, ssPassword.text);
        settingsController.setSetting(settingsController.keys.screenscraperDevId, ssDevId.text);
        settingsController.setSetting(settingsController.keys.screenscraperDevPassword, ssDevPassword.text);
        
        settingsController.setSetting(settingsController.keys.metadataProviderPriority, providerPriority.currentText);
        
        settingsController.setSetting(settingsController.keys.organizeNamingTemplate, namingTemplate.text);
        settingsController.setSetting(settingsController.keys.organizeBySystem, organizeBySystem.checked.toString());
        settingsController.setSetting(settingsController.keys.organizePreserveOriginals, preserveOriginals.checked.toString());
        
        settingsController.setSetting(settingsController.keys.performanceHashAlgorithm, hashAlgorithm.currentText);
        settingsController.setSetting(settingsController.keys.performanceParallelHashing, parallelHashing.checked.toString());
        
        console.log("Settings saved successfully");
    }
    
    function previewTemplate() {
        var template = namingTemplate.text;
        var preview = template
            .replace("{title}", "Super Mario World")
            .replace("{region}", "USA")
            .replace("{year}", "1990")
            .replace("{publisher}", "Nintendo")
            .replace("{disc}", "1")
            .replace("{id}", "12345");
        return preview;
    }
}
