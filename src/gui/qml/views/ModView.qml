import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

Item {
    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 14

        Label {
            text: "Mods"
            font.pixelSize: 26
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true

            TextField {
                id: modCatalogField
                Layout.fillWidth: true
                placeholderText: "Catalog URL or local JSON path"
                text: modController.catalogUrl
                onEditingFinished: modController.catalogUrl = text
            }

            Button {
                text: "Load"
                enabled: !modController.loadingCatalog
                onClicked: modController.loadCatalog(modCatalogField.text)
            }
        }

        TextField {
            id: modOutputField
            Layout.fillWidth: true
            placeholderText: "Output directory for installed mods"
        }

        Label {
            color: "#fb4934"
            text: modController.lastError
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Frame {
                SplitView.fillWidth: true
                ListView {
                    anchors.fill: parent
                    clip: true
                    spacing: 8
                    model: modListModel

                    delegate: Frame {
                        id: modDelegate

                        required property string modId
                        required property string title
                        required property string author
                        required property string version
                        required property string type
                        required property string format
                        required property bool installed
                        required property int installationId
                        required property string description

                        width: ListView.view.width
                        padding: 12
                        implicitHeight: contentColumn.implicitHeight + topPadding + bottomPadding

                        ColumnLayout {
                            id: contentColumn

                            x: modDelegate.leftPadding
                            y: modDelegate.topPadding
                            width: modDelegate.availableWidth
                            spacing: 6

                            Label { Layout.fillWidth: true; text: title; font.bold: true; elide: Text.ElideRight }
                            Label { Layout.fillWidth: true; text: author + " • v" + version + " • " + type + " • " + format; elide: Text.ElideRight }
                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: description
                            }

                            RowLayout {
                                Button {
                                    text: installed ? "Installed" : "Install"
                                    enabled: !installed && !modController.installing
                                    onClicked: modController.installMod(modId, modOutputField.text)
                                }
                                Button {
                                    text: "Uninstall"
                                    enabled: installed && !modController.installing
                                    onClicked: modController.uninstallInstallation(installationId)
                                }
                            }
                        }
                    }
                }
            }

            Frame {
                SplitView.preferredWidth: 280
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    Label {
                        text: "Installed Mods"
                        font.bold: true
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: modController.installedMods
                        delegate: Label {
                            required property var modelData
                            width: ListView.view.width
                            wrapMode: Text.WordWrap
                            text: modelData.title + " (" + modelData.version + ")"
                        }
                    }
                }
            }
        }
    }
}