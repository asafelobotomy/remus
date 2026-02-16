import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "components"

Page {
    id: artworkPage
    title: "Artwork"
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

    property var stats: ({})
    property var missingGames: []
    property int selectedGameId: -1

    Component.onCompleted: refreshStats()

    function refreshStats() {
        stats = artworkController.getArtworkStats()
        missingGames = artworkController.getGamesMissingArtwork("boxart", 50)
    }

    Connections {
        target: artworkController
        function onBatchDownloadCompleted(downloaded, failed) {
            refreshStats()
            statusLabel.text = "Downloaded: " + downloaded + " | Failed: " + failed
        }
        function onDownloadProgressChanged() {
            progressBar.value = artworkController.downloadProgress
        }
    }

    header: ToolBar {
        background: Rectangle { color: theme.cardBg }
        RowLayout {
            anchors.fill: parent

            Label {
                text: "Artwork Manager"
                font.pixelSize: 18
                font.bold: true
                color: theme.textPrimary
            }

            Item { Layout.fillWidth: true }

            ThemedButton {
                text: "Download All"
                isPrimary: true
                enabled: !artworkController.downloading
                onClicked: {
                    artworkController.downloadAllArtwork("", false)
                }
            }

            ThemedButton {
                text: "Cancel"
                visible: artworkController.downloading
                onClicked: artworkController.cancelDownloads()
            }

            ThemedButton {
                text: "Refresh"
                onClicked: refreshStats()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        // Statistics cards
        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            Rectangle {
                Layout.preferredWidth: 180
                Layout.preferredHeight: 100
                color: theme.cardBg
                radius: 8

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 5

                    Label {
                        text: stats.totalGames || 0
                        font.pixelSize: 32
                        font.bold: true
                        color: theme.primary
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Label {
                        text: "Total Games"
                        color: theme.textSecondary
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 180
                Layout.preferredHeight: 100
                color: theme.cardBg
                radius: 8

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 5

                    Label {
                        text: stats.withArtwork || 0
                        font.pixelSize: 32
                        font.bold: true
                        color: theme.success
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Label {
                        text: "With Artwork"
                        color: theme.textSecondary
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 180
                Layout.preferredHeight: 100
                color: theme.cardBg
                radius: 8

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 5

                    Label {
                        text: stats.missingArtwork || 0
                        font.pixelSize: 32
                        font.bold: true
                        color: theme.warning
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Label {
                        text: "Missing"
                        color: theme.textSecondary
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 200
                Layout.preferredHeight: 100
                color: theme.cardBg
                radius: 8

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 5

                    Label {
                        text: (stats.storageUsedMB || 0).toFixed(1) + " MB"
                        font.pixelSize: 24
                        font.bold: true
                        color: theme.info
                        Layout.alignment: Qt.AlignHCenter
                    }
                    Label {
                        text: "Storage Used"
                        color: theme.textSecondary
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        // Progress bar (visible during download)
        Rectangle {
            Layout.fillWidth: true
            height: 60
            color: theme.cardBg
            radius: 8
            visible: artworkController.downloading

            RowLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 15

                Label {
                    text: "Downloading artwork..."
                    color: theme.textPrimary
                }

                ThemedProgressBar {
                    id: progressBar
                    Layout.fillWidth: true
                    from: 0
                    to: artworkController.downloadTotal
                    value: artworkController.downloadProgress
                }

                Label {
                    text: artworkController.downloadProgress + " / " + artworkController.downloadTotal
                    color: theme.textSecondary
                }
            }
        }

        // Status label
        Label {
            id: statusLabel
            color: theme.textSecondary
            visible: text.length > 0
        }

        // Main content: artwork gallery grid
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: theme.cardBg
            radius: 8

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 10

                Label {
                    text: "Games Missing Artwork"
                    font.bold: true
                    font.pixelSize: 14
                    color: theme.textPrimary
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    GridView {
                        id: artworkGrid
                        width: parent.width
                        cellWidth: 180
                        cellHeight: 240

                        model: missingGames

                        delegate: Rectangle {
                            width: 170
                            height: 230
                            color: selectedGameId === modelData.id ? theme.navActiveBg : theme.contentBg
                            radius: 6
                            border.color: selectedGameId === modelData.id ? theme.primary : theme.border
                            border.width: 1

                            MouseArea {
                                anchors.fill: parent
                                onClicked: selectedGameId = modelData.id
                                onDoubleClicked: {
                                    // Download artwork for this game
                                    artworkController.downloadArtwork(modelData.id)
                                }
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                // Placeholder for artwork
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 150
                                    color: theme.mainBg
                                    radius: 4

                                    Label {
                                        anchors.centerIn: parent
                                        text: "No\nArtwork"
                                        color: theme.textMuted
                                        horizontalAlignment: Text.AlignHCenter
                                    }

                                    // Show artwork if available
                                    Image {
                                        anchors.fill: parent
                                        source: artworkController.getArtworkUrl(modelData.id, "boxart")
                                        fillMode: Image.PreserveAspectFit
                                        visible: source != ""
                                        asynchronous: true
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.title || "Unknown"
                                    elide: Text.ElideRight
                                    font.bold: true
                                    font.pixelSize: 11
                                    color: theme.textPrimary
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.system || ""
                                    color: theme.textSecondary
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }

        // Artwork path info
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "Artwork folder:"
                color: theme.textSecondary
            }

            Label {
                text: stats.artworkPath || "~/.local/share/Remus/artwork"
                color: theme.textMuted
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }

            ThemedButton {
                text: "Clear Cache"
                onClicked: {
                    artworkController.clearArtworkCache()
                    refreshStats()
                }
            }
        }
    }
}
