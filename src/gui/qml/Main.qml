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

    color: "#1d2021"
    palette.window: "#1d2021"
    palette.windowText: "#ebdbb2"
    palette.base: "#282828"
    palette.alternateBase: "#32302f"
    palette.text: "#ebdbb2"
    palette.button: "#3c3836"
    palette.buttonText: "#fbf1c7"
    palette.brightText: "#fbf1c7"
    palette.highlight: "#458588"
    palette.highlightedText: "#fbf1c7"
    palette.placeholderText: "#a89984"
    palette.toolTipBase: "#3c3836"
    palette.toolTipText: "#fbf1c7"
    palette.link: "#83a598"
    palette.dark: "#282828"
    palette.mid: "#504945"
    palette.midlight: "#665c54"
    palette.light: "#665c54"

    background: Rectangle {
        color: "#1d2021"
    }

    header: ToolBar {
        contentHeight: topBarLayout.implicitHeight + 12

        background: Rectangle {
            color: "#282828"
            border.color: "#504945"
        }

        ColumnLayout {
            id: topBarLayout

            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

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
                            settingsController.setValue("gui/default_library_path", libraryField.text)
                        }
                    }
                }

                Label {
                    visible: appController.libraryOpen
                    text: "Remus"
                    font.pixelSize: 18
                    font.bold: true
                    color: "#fbf1c7"
                }
            }

            ActionToolbar {
                Layout.fillWidth: true
                visible: appController.libraryOpen

                onPipelineDrawerRequested: libraryWorkbench.openPipelineDrawer()
                onRunAllRequested:         libraryWorkbench.requestRunAll()
                onUtilitiesRequested:        appController.currentView = 1
                onSettingsRequested:         appController.currentView = 2
                onEditRequested:             libraryWorkbench.openEditDialog()
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
            onViewRequested: function(index) {
                appController.currentView = index
            }
        }

        Frame {
            SplitView.fillWidth: true
            SplitView.fillHeight: true

            background: Rectangle {
                radius: 24
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#282828" }
                    GradientStop { position: 1.0; color: "#32302f" }
                }
                border.color: "#504945"
            }

            StackLayout {
                anchors.fill: parent
                currentIndex: appController.currentView

                LibraryWorkbench {
                    id: libraryWorkbench
                }
                UtilitiesView {}
                SettingsView  {}
            }
        }
    }
}
