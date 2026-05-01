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
            text: "Artwork"
            font.pixelSize: 26
            font.bold: true
        }

        RowLayout {
            Button {
                text: "Refresh"
                enabled: appController.selectedFileId > 0
                onClicked: artworkController.refreshSelectedArtwork()
            }
            Button {
                text: "Download Selected"
                enabled: appController.selectedFileId > 0 && !artworkController.downloading
                onClicked: artworkController.downloadSelected()
            }
            Button {
                text: "Download All Matched"
                enabled: !artworkController.downloading
                onClicked: artworkController.downloadAllMatched()
            }
        }

        ProgressCard {
            title: "Artwork Progress"
            progressValue: artworkController.downloadProgress
            progressTotal: artworkController.downloadTotal
            message: artworkController.lastError.length > 0 ? artworkController.lastError : artworkController.localArtworkPath
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

                Image {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    fillMode: Image.PreserveAspectFit
                    source: artworkController.previewUrl
                    asynchronous: true
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: artworkController.previewUrl.toString().length > 0
                        ? artworkController.previewUrl.toString()
                        : "No artwork loaded for the current selection."
                }
            }
        }
    }
}