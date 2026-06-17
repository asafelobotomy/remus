import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

Frame {
    Layout.fillWidth: true

    background: Rectangle {
        radius: 16
        color: Theme.surfaceAlt
        border.color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            text: metadataEditorController.currentGame.title || "No game selected"
            font.bold: true
            font.pixelSize: Theme.fontTitle
            color: Theme.textPrimary
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            Label { text: "Publisher" }
            TextField {
                Layout.fillWidth: true
                enabled: metadataEditorController.currentGame.gameId > 0
                text: metadataEditorController.currentGame.publisher || ""
                onEditingFinished: metadataEditorController.setField("publisher", text)
            }

            Label { text: "Developer" }
            TextField {
                Layout.fillWidth: true
                enabled: metadataEditorController.currentGame.gameId > 0
                text: metadataEditorController.currentGame.developer || ""
                onEditingFinished: metadataEditorController.setField("developer", text)
            }

            Label { text: "Release" }
            TextField {
                Layout.fillWidth: true
                enabled: metadataEditorController.currentGame.gameId > 0
                text: metadataEditorController.currentGame.releaseDate || ""
                onEditingFinished: metadataEditorController.setField("releaseDate", text)
            }

            Label { text: "Genres" }
            TextField {
                Layout.fillWidth: true
                enabled: metadataEditorController.currentGame.gameId > 0
                text: metadataEditorController.currentGame.genres || ""
                onEditingFinished: metadataEditorController.setField("genres", text)
            }

            Label { text: "Players" }
            TextField {
                Layout.fillWidth: true
                enabled: metadataEditorController.currentGame.gameId > 0
                text: metadataEditorController.currentGame.players || ""
                onEditingFinished: metadataEditorController.setField("players", text)
            }

            Label { text: "Rating" }
            TextField {
                Layout.fillWidth: true
                enabled: metadataEditorController.currentGame.gameId > 0
                text: metadataEditorController.currentGame.rating || ""
                onEditingFinished: metadataEditorController.setField("rating", text)
            }
        }

        Label { text: "Description" }
        TextArea {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            enabled: metadataEditorController.currentGame.gameId > 0
            text: metadataEditorController.currentGame.description || ""
            wrapMode: Text.Wrap
            onEditingFinished: metadataEditorController.setField("description", text)
        }

        RowLayout {
            Button {
                text: "Save"
                enabled: metadataEditorController.dirty
                onClicked: metadataEditorController.save()
            }
            Button {
                text: "Discard"
                enabled: metadataEditorController.dirty
                onClicked: metadataEditorController.discard()
            }
        }
    }
}