import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * @brief First-run setup wizard for optional ScreenScraper integration
 * 
 * Phase 2: Online Enhancement
 * - Optional free-tier ScreenScraper account setup
 * - Explains benefits: artwork + extended descriptions
 * - Can be skipped (uses LocalDatabase only)
 */
Dialog {
    id: setupWizard
    title: "Welcome to Remus!"
    modal: true
    width: 600
    height: 500
    anchors.centerIn: parent
    
    property int currentPage: 0
    
    background: Rectangle {
        color: theme.cardBg
        border.color: theme.border
        border.width: 1
        radius: 8
    }
    
    header: Rectangle {
        height: 60
        color: theme.mainBg
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 15
            spacing: 5
            
            Label {
                text: setupWizard.title
                font.pixelSize: 18
                font.bold: true
                color: theme.textPrimary
            }
            
            Label {
                text: "Setup your ROM library manager"
                font.pixelSize: 12
                color: theme.textSecondary
            }
        }
    }
    
    contentItem: StackLayout {
        id: pageStack
        currentIndex: setupWizard.currentPage
        
        // Page 0: Welcome
        Item {
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 80
                spacing: 20
                
                Label {
                    text: "🎮 Welcome to Remus"
                    font.pixelSize: 24
                    font.bold: true
                    color: theme.primary
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Label {
                    text: "Your offline-first ROM library manager"
                    font.pixelSize: 14
                    color: theme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 2
                    color: theme.border
                    Layout.topMargin: 10
                    Layout.bottomMargin: 10
                }
                
                Label {
                    text: "✅ Offline ROM identification (No-Intro/Redump databases)\n" +
                          "✅ Automatic hash-based matching (CRC32/MD5/SHA1)\n" +
                          "✅ Organize your library with custom templates\n" +
                          "✅ Verify and patch ROMs\n" +
                          "✅ Export to RetroArch, EmulationStation"
                    font.pixelSize: 13
                    color: theme.textPrimary
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    lineHeight: 1.5
                }
                
                Label {
                    text: "Let's get you set up in just a few steps!"
                    font.pixelSize: 13
                    font.bold: true
                    color: theme.primary
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 20
                }
            }
        }
        
        // Page 1: ScreenScraper Setup (Optional)
        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 30
                spacing: 15
                
                Label {
                    text: "📦 Optional: Enhanced Metadata"
                    font.pixelSize: 20
                    font.bold: true
                    color: theme.primary
                }
                
                Label {
                    text: "Remus works 100% offline using local DAT files (No-Intro/Redump).\n\n" +
                          "For enhanced features, you can optionally add a FREE ScreenScraper.fr account:"
                    font.pixelSize: 12
                    color: theme.textPrimary
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }
                
                // Benefits list
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    
                    Label {
                        text: "📸 Box art, screenshots, and logos"
                        font.pixelSize: 12
                        color: theme.textPrimary
                    }
                    
                    Label {
                        text: "📝 Extended game descriptions and metadata"
                        font.pixelSize: 12
                        color: theme.textPrimary
                    }
                    
                    Label {
                        text: "🎯 Publisher, developer, and release date info"
                        font.pixelSize: 12
                        color: theme.textPrimary
                    }
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }
                
                Label {
                    text: "⚡ Free tier: 10,000 requests/day (more than enough!)\n" +
                          "💾 All data is cached locally - fetched once, stored forever\n" +
                          "🔒 Your credentials are stored securely on your device"
                    font.pixelSize: 11
                    color: theme.textSecondary
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.bold: true
                }
                
                Item { Layout.preferredHeight: 10 }
                
                // Credentials form
                GroupBox {
                    title: "ScreenScraper Credentials (Optional)"
                    Layout.fillWidth: true
                    
                    background: Rectangle {
                        color: theme.mainBg
                        border.color: theme.border
                        border.width: 1
                        radius: 4
                    }
                    
                    label: Label {
                        text: parent.title
                        font.bold: true
                        color: theme.textPrimary
                        padding: 5
                    }
                    
                    GridLayout {
                        anchors.fill: parent
                        columns: 2
                        columnSpacing: 10
                        rowSpacing: 10
                        
                        Label {
                            text: "Username:"
                            color: theme.textPrimary
                        }
                        
                        ThemedTextField {
                            id: wizardSsUsername
                            Layout.fillWidth: true
                            placeholderText: "Leave empty to skip"
                        }
                        
                        Label {
                            text: "Password:"
                            color: theme.textPrimary
                        }
                        
                        ThemedTextField {
                            id: wizardSsPassword
                            Layout.fillWidth: true
                            placeholderText: "Leave empty to skip"
                            echoMode: TextInput.Password
                        }
                    }
                }
                
                Label {
                    text: "Don't have an account? <a href='https://www.screenscraper.fr/'>Create one FREE at ScreenScraper.fr</a>"
                    font.pixelSize: 11
                    color: theme.textSecondary
                    Layout.fillWidth: true
                    onLinkActivated: Qt.openUrlExternally(link)
                    textFormat: Text.RichText
                }
                
                Item { Layout.fillHeight: true }
            }
        }
        
        // Page 2: Complete
        Item {
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 80
                spacing: 20
                
                Label {
                    text: "✅ Setup Complete!"
                    font.pixelSize: 24
                    font.bold: true
                    color: theme.success
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Label {
                    text: wizardSsUsername.text.length > 0 ?
                          "ScreenScraper integration enabled!\nArtwork and extended metadata will be fetched automatically." :
                          "Offline mode enabled!\nRemus will use local DAT files for ROM identification."
                    font.pixelSize: 13
                    color: theme.textPrimary
                    Layout.alignment: Qt.AlignHCenter
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 2
                    color: theme.border
                    Layout.topMargin: 15
                    Layout.bottomMargin: 15
                }
                
                Label {
                    text: "🚀 You're ready to start managing your ROM library!"
                    font.pixelSize: 14
                    font.bold: true
                    color: theme.primary
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Label {
                    text: "Click 'Finish' to begin scanning and organizing your ROMs."
                    font.pixelSize: 12
                    color: theme.textSecondary
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Label {
                    text: "💡 Tip: You can always change these settings later in Settings → Metadata Providers"
                    font.pixelSize: 11
                    color: theme.textMuted
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 20
                    font.italic: true
                }
            }
        }
    }
    
    footer: DialogButtonBox {
        background: Rectangle {
            color: theme.mainBg
        }
        
        ThemedButton {
            text: setupWizard.currentPage === 0 ? "Next" : 
                  (setupWizard.currentPage === 1 ? "Next" : "Finish")
            isPrimary: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            visible: true
            
            onClicked: {
                if (setupWizard.currentPage < 2) {
                    setupWizard.currentPage++
                } else {
                    setupWizard.completeSetup()
                }
            }
        }
        
        ThemedButton {
            text: setupWizard.currentPage === 1 ? "Skip" : "Back"
            visible: setupWizard.currentPage > 0
            
            onClicked: {
                if (setupWizard.currentPage === 1) {
                    // Skip ScreenScraper setup
                    setupWizard.currentPage = 2
                } else if (setupWizard.currentPage > 0) {
                    setupWizard.currentPage--
                }
            }
        }
    }
    
    function completeSetup() {
        // Save ScreenScraper credentials if provided
        if (wizardSsUsername.text.length > 0 && wizardSsPassword.text.length > 0) {
            settingsController.setValue("providers/screenscraper/username", wizardSsUsername.text)
            settingsController.setValue("providers/screenscraper/password", wizardSsPassword.text)
            console.log("SetupWizard: ScreenScraper credentials saved")
        }
        
        // Mark first-run as complete
        settingsController.setValue("app/first_run_complete", true)
        console.log("SetupWizard: First-run setup completed")
        
        // Close wizard
        setupWizard.accept()
        
        // Notify user to restart if credentials were added
        if (wizardSsUsername.text.length > 0) {
            // Request app restart to load ScreenScraper provider
            Qt.quit() // Simple approach: close app, user restarts
        }
    }
}
