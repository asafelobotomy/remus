import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

ApplicationWindow {
    id: window

    width: 1480
    height: 920
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    title: "Remus"

    color: Theme.background
    palette.window: Theme.background
    palette.windowText: Theme.textBody
    palette.base: Theme.surface
    palette.alternateBase: Theme.surfaceAlt
    palette.text: Theme.textBody
    palette.button: "#3c3836"
    palette.buttonText: Theme.textPrimary
    palette.brightText: Theme.textPrimary
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.textPrimary
    palette.placeholderText: Theme.textMuted
    palette.toolTipBase: "#3c3836"
    palette.toolTipText: Theme.textPrimary
    palette.link: Theme.accentAlt
    palette.dark: Theme.surface
    palette.mid: Theme.border
    palette.midlight: Theme.textDisabled
    palette.light: Theme.textDisabled

    background: Rectangle {
        color: Theme.background
    }

    header: ToolBar {
        contentHeight: topBarLayout.implicitHeight + 12

        background: Rectangle {
            color: Theme.surface
            border.color: Theme.border
        }

        ColumnLayout {
            id: topBarLayout

            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Label {
                    text: "Database"
                    font.pixelSize: Theme.fontSm
                    color: Theme.textDim
                }

                TextField {
                    id: libraryField
                    Layout.preferredWidth: 320
                    Layout.fillWidth: true
                    placeholderText: appController.defaultLibraryPath()
                    text: settingsController.stringValue("gui/default_library_path", appController.defaultLibraryPath())
                    onEditingFinished: settingsController.setValue("gui/default_library_path", text)
                }

                Button {
                    text: "Open Library"
                    onClicked: {
                        if (appController.openLibrary(libraryField.text)) {
                            settingsController.setValue("gui/default_library_path", libraryField.text);
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Label {
                    visible: appController.libraryOpen
                    text: appController.statusMessage
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSm
                    elide: Text.ElideRight
                    Layout.maximumWidth: 280
                }
            }

            ActionToolbar {
                id: actionToolbar
                Layout.fillWidth: true

                onPipelineDrawerRequested: pipelineDrawer.open()
                onRunAllRequested: libraryWorkbench.requestRunAll()
                onSettingsRequested: appController.currentView = 1
                onUtilityToolRequested: function (tab) {
                    utilitiesPanel.openTab(tab);
                    utilitiesDrawer.open();
                }
                onEditRequested: libraryWorkbench.openEditDialog()
                onMatchEnrichRequested: libraryWorkbench.openMatchEnrichDialog()
                onRenameOrganizeRequested: libraryWorkbench.openRenameOrganizeDialog()
            }
        }
    }

    SplitView {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 18

        Sidebar {
            SplitView.preferredWidth: 220
            currentIndex: appController.currentView
            onViewRequested: function (index) {
                appController.currentView = index;
            }
        }

        Frame {
            SplitView.fillWidth: true
            SplitView.fillHeight: true

            background: Rectangle {
                radius: 24
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: Theme.surface
                    }
                    GradientStop {
                        position: 1.0
                        color: Theme.surfaceAlt
                    }
                }
                border.color: Theme.border
            }

            StackLayout {
                anchors.fill: parent
                currentIndex: appController.currentView

                LibraryWorkbench {
                    id: libraryWorkbench
                }
                SettingsView {}
            }
        }
    }

    Drawer {
        id: pipelineDrawer
        parent: Overlay.overlay
        edge: Qt.RightEdge
        width: Math.min(window.width * 0.55, 640)
        height: window.height
        modal: true
        interactive: true

        background: Rectangle {
            color: Theme.surface
            border.color: Theme.border
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            ToolBar {
                Layout.fillWidth: true

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    Label {
                        text: "Pipeline Stages"
                        font.bold: true
                        color: Theme.textPrimary
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    ToolButton {
                        text: "Close"
                        onClicked: pipelineDrawer.close()
                    }
                }
            }

            PipelinePanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }

    Drawer {
        id: utilitiesDrawer
        parent: Overlay.overlay
        edge: Qt.RightEdge
        width: Math.min(window.width * 0.55, 720)
        height: window.height
        modal: true
        interactive: true

        background: Rectangle {
            color: Theme.surface
            border.color: Theme.border
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            ToolBar {
                Layout.fillWidth: true

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    Label {
                        text: "Tools"
                        font.bold: true
                        color: Theme.textPrimary
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    ToolButton {
                        text: "Close"
                        onClicked: utilitiesDrawer.close()
                    }
                }
            }

            UtilitiesView {
                id: utilitiesPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
        }
    }

    ErrorBanner {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 18
        message: appController.errorMessage
    }

    Connections {
        target: scanController
        function onScanError(message) {
            appController.showError(message);
        }
    }

    Connections {
        target: hashController
        function onHashError(message) {
            appController.showError(message);
        }
    }

    Connections {
        target: matchController
        function onMatchError(message) {
            appController.showError(message);
        }
    }
}
