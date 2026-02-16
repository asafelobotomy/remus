import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "components"

Page {
    id: gameDetailsPage
    title: "Game Details"
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

    property int gameId: -1
    property var gameData: ({})
    property var metadataSources: []
    property var gameFiles: []
    property bool editMode: false

    Component.onCompleted: {
        if (gameId > 0) {
            loadGame()
        }
    }

    function loadGame() {
        gameData = metadataEditor.getGameDetails(gameId)
        metadataSources = metadataEditor.getMetadataSources(gameId)
        gameFiles = metadataEditor.getGameFiles(gameId)
    }

    function saveChanges() {
        metadataEditor.saveChanges()
        editMode = false
        loadGame()
    }

    function cancelEdit() {
        metadataEditor.discardChanges()
        editMode = false
        loadGame()
    }

    header: ToolBar {
        background: Rectangle { color: theme.cardBg }
        RowLayout {
            anchors.fill: parent

            ThemedButton {
                text: "Back"
                icon.name: "go-previous"
                onClicked: stackView.pop()
            }

            Label {
                text: gameData.title || "Game Details"
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
                color: theme.textPrimary
            }

            ThemedButton {
                text: editMode ? "Save" : "Edit"
                icon.name: editMode ? "document-save" : "document-edit"
                isPrimary: editMode
                onClicked: {
                    if (editMode) {
                        saveChanges()
                    } else {
                        editMode = true
                    }
                }
            }

            ThemedButton {
                text: "Cancel"
                visible: editMode
                onClicked: cancelEdit()
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 20
        clip: true

        ColumnLayout {
            width: parent.width - 40
            spacing: 20

            // Top section: Artwork + Basic Info
            RowLayout {
                Layout.fillWidth: true
                spacing: 20

                // Artwork preview
                Rectangle {
                    Layout.preferredWidth: 200
                    Layout.preferredHeight: 280
                    color: theme.cardBg
                    radius: 8

                    Image {
                        anchors.fill: parent
                        anchors.margins: 10
                        source: gameId > 0 ? artworkController.getArtworkUrl(gameId, "boxart") : ""
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true

                        Rectangle {
                            anchors.fill: parent
                            color: theme.mainBg
                            visible: parent.status !== Image.Ready

                            Label {
                                anchors.centerIn: parent
                                text: "No\nArtwork"
                                color: theme.textMuted
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }

                    ThemedButton {
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        anchors.margins: 5
                        text: "Download"
                        flat: true
                        font.pixelSize: 10
                        onClicked: artworkController.downloadArtwork(gameId)
                    }
                }

                // Basic info
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 280
                    color: theme.cardBg
                    radius: 8

                    GridLayout {
                        anchors.fill: parent
                        anchors.margins: 15
                        columns: 2
                        columnSpacing: 15
                        rowSpacing: 10

                        Label { text: "Title:"; font.bold: true; color: theme.textPrimary }
                        ThemedEditableField {
                            Layout.fillWidth: true
                            text: gameData.title || ""
                            readOnly: !editMode
                            onTextChanged: {
                                if (editMode && text !== gameData.title) {
                                    metadataEditor.updateField(gameId, "title", text)
                                }
                            }
                        }

                        Label { text: "System:"; font.bold: true; color: theme.textPrimary }
                        ThemedEditableField {
                            Layout.fillWidth: true
                            text: gameData.system || ""
                            readOnly: !editMode
                            onTextChanged: {
                                if (editMode && text !== gameData.system) {
                                    metadataEditor.updateField(gameId, "system", text)
                                }
                            }
                        }

                        Label { text: "Region:"; font.bold: true; color: theme.textPrimary }
                        ThemedEditableField {
                            Layout.fillWidth: true
                            text: gameData.region || ""
                            readOnly: !editMode
                            onTextChanged: {
                                if (editMode && text !== gameData.region) {
                                    metadataEditor.updateField(gameId, "region", text)
                                }
                            }
                        }

                        Label { text: "Year:"; font.bold: true; color: theme.textPrimary }
                        ThemedEditableField {
                            Layout.fillWidth: true
                            text: gameData.year || ""
                            readOnly: !editMode
                            onTextChanged: {
                                if (editMode && text !== gameData.year) {
                                    metadataEditor.updateField(gameId, "year", text)
                                }
                            }
                        }

                        Label { text: "Publisher:"; font.bold: true; color: theme.textPrimary }
                        ThemedEditableField {
                            Layout.fillWidth: true
                            text: gameData.publisher || ""
                            readOnly: !editMode
                            onTextChanged: {
                                if (editMode && text !== gameData.publisher) {
                                    metadataEditor.updateField(gameId, "publisher", text)
                                }
                            }
                        }

                        Label { text: "Developer:"; font.bold: true; color: theme.textPrimary }
                        ThemedEditableField {
                            Layout.fillWidth: true
                            text: gameData.developer || ""
                            readOnly: !editMode
                            onTextChanged: {
                                if (editMode && text !== gameData.developer) {
                                    metadataEditor.updateField(gameId, "developer", text)
                                }
                            }
                        }

                        Label { text: "Genre:"; font.bold: true; color: theme.textPrimary }
                        ThemedEditableField {
                            Layout.fillWidth: true
                            text: gameData.genre || ""
                            readOnly: !editMode
                            onTextChanged: {
                                if (editMode && text !== gameData.genre) {
                                    metadataEditor.updateField(gameId, "genre", text)
                                }
                            }
                        }

                        Label { text: "Players:"; font.bold: true; color: theme.textPrimary }
                        ThemedEditableField {
                            Layout.fillWidth: true
                            text: gameData.players || ""
                            readOnly: !editMode
                            onTextChanged: {
                                if (editMode && text !== gameData.players) {
                                    metadataEditor.updateField(gameId, "players", text)
                                }
                            }
                        }
                    }
                }
            }

            // Description section
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                color: theme.cardBg
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10

                    Label {
                        text: "Description"
                        font.bold: true
                        color: theme.textPrimary
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ThemedEditableTextArea {
                            text: gameData.description || "No description available."
                            readOnly: !editMode
                            onTextChanged: {
                                if (editMode && text !== gameData.description) {
                                    metadataEditor.updateField(gameId, "description", text)
                                }
                            }
                        }
                    }
                }
            }

            // Associated files section
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                color: theme.cardBg
                radius: 8

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: "Associated Files (" + gameFiles.length + ")"
                            font.bold: true
                            color: theme.textPrimary
                        }

                        Item { Layout.fillWidth: true }

                        Label {
                            text: "Confidence: " + (gameData.confidence || 0) + "%"
                            color: {
                                var c = gameData.confidence || 0
                                if (c >= 90) return theme.success
                                if (c >= 60) return theme.warning
                                return theme.danger
                            }
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: gameFiles

                        delegate: Rectangle {
                            width: parent.width
                            height: 40
                            color: index % 2 === 0 ? theme.contentBg : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 5
                                spacing: 10

                                Label {
                                    text: modelData.filename || ""
                                    Layout.fillWidth: true
                                    elide: Text.ElideMiddle
                                    color: theme.textPrimary
                                }

                                Label {
                                    text: (modelData.size / (1024 * 1024)).toFixed(2) + " MB"
                                    color: theme.textSecondary
                                }

                                Label {
                                    text: modelData.matchType || ""
                                    color: theme.textMuted
                                    font.pixelSize: 11
                                }

                                ThemedCheckBox {
                                    checked: modelData.userConfirmed || false
                                    text: "Confirmed"
                                    enabled: editMode
                                    onClicked: {
                                        // Would call metadataEditor.setMatchConfirmation()
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Metadata sources section
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                color: theme.cardBg
                radius: 8
                visible: metadataSources.length > 0

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 15
                    spacing: 10

                    Label {
                        text: "Metadata Sources"
                        font.bold: true
                        color: theme.textPrimary
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: metadataSources

                        delegate: Rectangle {
                            width: parent.width
                            height: 35
                            color: index % 2 === 0 ? theme.contentBg : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 5
                                spacing: 10

                                Label {
                                    text: modelData.providerName || ""
                                    font.bold: true
                                    Layout.preferredWidth: 120
                                    color: theme.textPrimary
                                }

                                Label {
                                    text: modelData.title || ""
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    color: theme.textPrimary
                                }

                                Label {
                                    text: "Priority: " + (modelData.priority || 0)
                                    color: theme.textSecondary
                                    font.pixelSize: 11
                                }

                                Label {
                                    text: modelData.fetchedAt || ""
                                    color: theme.textMuted
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }
                }
            }

            // Status bar
            RowLayout {
                Layout.fillWidth: true
                visible: metadataEditor.hasChanges

                Rectangle {
                    Layout.fillWidth: true
                    height: 40
                    color: theme.warning
                    radius: 4

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10

                        Label {
                            text: "You have unsaved changes"
                            color: theme.textPrimary
                        }

                        Item { Layout.fillWidth: true }

                        ThemedButton {
                            text: "Save"
                            isPrimary: true
                            onClicked: saveChanges()
                        }

                        ThemedButton {
                            text: "Discard"
                            onClicked: cancelEdit()
                        }
                    }
                }
            }
        }
    }
}
