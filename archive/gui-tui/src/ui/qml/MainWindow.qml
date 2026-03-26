import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "components"

ApplicationWindow {
    id: mainWindow
    width: 1200
    height: 800
    visible: true
    title: "Remus - ROM Library Manager"
    color: theme.mainBg
    palette.window: theme.mainBg
    palette.windowText: theme.textPrimary
    palette.base: theme.mainBg
    palette.text: theme.textPrimary
    palette.button: theme.mainBg
    palette.buttonText: theme.textPrimary
    palette.highlight: theme.primary
    palette.highlightedText: theme.textLight
    palette.placeholderText: theme.textMuted
    
    Component.onCompleted: {
        // Check for first run and show setup wizard
        if (settingsController.isFirstRun()) {
            setupWizardLoader.active = true
        }
    }
    
    // First-run setup wizard
    Loader {
        id: setupWizardLoader
        active: false
        sourceComponent: SetupWizard {
            onAccepted: {
                setupWizardLoader.active = false
            }
            onRejected: {
                // User closed wizard without completing
                // Mark as complete anyway to not show again
                settingsController.markFirstRunComplete()
                setupWizardLoader.active = false
            }
        }
    }
    
    // Sidebar
    Rectangle {
        id: sidebar
        width: 250
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: theme.sidebarBg
        
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 5
            
            // Logo/Title
            Label {
                text: "REMUS"
                font.pixelSize: 24
                font.bold: true
                color: theme.navText
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 20
                Layout.bottomMargin: 20
            }
            
            // Navigation buttons
            Button {
                text: "Library"
                Layout.fillWidth: true
                flat: true
                highlighted: stackView.currentItem === libraryView
                contentItem: Label {
                    text: parent.text
                    color: parent.highlighted ? theme.navActive : theme.navText
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: stackView.replace(libraryView)
            }
            
            Button {
                text: "Artwork"
                Layout.fillWidth: true
                flat: true
                highlighted: stackView.currentItem === artworkView
                contentItem: Label {
                    text: parent.text
                    color: parent.highlighted ? theme.navActive : theme.navText
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: stackView.replace(artworkView)
            }
            
            Button {
                text: "Verification"
                Layout.fillWidth: true
                flat: true
                highlighted: stackView.currentItem === verificationView
                contentItem: Label {
                    text: parent.text
                    color: parent.highlighted ? theme.navActive : theme.navText
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: stackView.replace(verificationView)
            }
            
            Button {
                text: "Patching"
                Layout.fillWidth: true
                flat: true
                highlighted: stackView.currentItem === patchView
                contentItem: Label {
                    text: parent.text
                    color: parent.highlighted ? theme.navActive : theme.navText
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: stackView.replace(patchView)
            }
            
            Button {
                text: "Export"
                Layout.fillWidth: true
                flat: true
                highlighted: stackView.currentItem === exportView
                contentItem: Label {
                    text: parent.text
                    color: parent.highlighted ? theme.navActive : theme.navText
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: stackView.replace(exportView)
            }
            
            Button {
                text: "Settings"
                Layout.fillWidth: true
                flat: true
                highlighted: stackView.currentItem === settingsView
                contentItem: Label {
                    text: parent.text
                    color: parent.highlighted ? theme.navActive : theme.navText
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: stackView.replace(settingsView)
            }
            
            Item {
                Layout.fillHeight: true
            }
            
            // Stats panel
            Rectangle {
                Layout.fillWidth: true
                height: 120
                color: theme.navHover
                radius: 5
                
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    
                    Label {
                        text: "Library Stats"
                        font.bold: true
                        color: theme.navText
                    }
                    
                    Label {
                        text: "Total Files: 0"
                        color: theme.textLight
                        font.bold: true
                    }
                    
                    Label {
                        text: "Matched: 0"
                        color: theme.textLight
                        font.bold: true
                    }
                    
                    Label {
                        text: "Systems: 0"
                        color: theme.textLight
                        font.bold: true
                    }
                }
            }
        }
    }
    
    // Main content area
    StackView {
        id: stackView
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        initialItem: libraryView
        
        Component {
            id: libraryView
            LibraryView {}
        }
        
        Component {
            id: matchReviewView
            MatchReviewView {}
        }
        
        Component {
            id: conversionView
            ConversionView {}
        }
        
        Component {
            id: artworkView
            ArtworkView {}
        }
        
        Component {
            id: verificationView
            VerificationView {}
        }
        
        Component {
            id: patchView
            PatchView {}
        }
        
        Component {
            id: exportView
            ExportView {}
        }
        
        Component {
            id: settingsView
            SettingsView {}
        }
    }
    
    // Status bar
    footer: ToolBar {
        background: Rectangle { color: theme.cardBg }
        RowLayout {
            anchors.fill: parent
            
            Label {
                text: libraryController.scanStatus
                Layout.fillWidth: true
                color: theme.textPrimary
                font.bold: true
            }
            
            ThemedProgressBar {
                visible: libraryController.scanning
                from: 0
                to: libraryController.scanTotal
                value: libraryController.scanProgress
                Layout.preferredWidth: 200
            }
            
            BusyIndicator {
                running: libraryController.scanning || libraryController.hashing
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
            }
        }
    }
}
