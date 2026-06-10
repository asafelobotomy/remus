import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

// TMM-style match search dialog: results list, preview pane, field import toggles.
Dialog {
    id: dialog

    property bool downloadArtworkOnApply: true

    title: "Match & Enrich"
    modal: true
    anchors.centerIn: Overlay.overlay
    width:  Math.min(920, Overlay.overlay.width * 0.92)
    height: Math.min(680, Overlay.overlay.height * 0.88)

    onOpened: {
        if (appController.selectedFileId > 0)
            matchController.beginSearch(appController.selectedFileId)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            color: "#928374"
            text: matchController.searchRomPath.length > 0
                  ? matchController.searchRomPath
                  : "No ROM selected"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Provider"
                color: "#a89984"
                font.pixelSize: 11
            }
            ComboBox {
                id: providerCombo
                Layout.preferredWidth: 160
                model: matchController.enabledProviders()
                font.pixelSize: 12
                Component.onCompleted: {
                    const idx = model.indexOf(matchController.searchProvider)
                    if (idx >= 0)
                        currentIndex = idx
                }
                onActivated: matchController.searchProvider = model[currentIndex]
            }

            Label {
                text: "System"
                color: "#a89984"
                font.pixelSize: 11
            }
            Label {
                Layout.preferredWidth: 120
                elide: Text.ElideRight
                text: matchController.searchSystem || "—"
                color: "#ebdbb2"
                font.pixelSize: 11
            }

            TextField {
                id: queryField
                Layout.fillWidth: true
                placeholderText: "Search query…"
                text: matchController.searchQuery
                font.pixelSize: 12
                onAccepted: runSearch()
            }

            Button {
                text: matchController.searching ? "Searching…" : "Search"
                enabled: !matchController.searching && queryField.text.trim().length > 0
                onClicked: runSearch()
            }
        }

        SplitView {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            orientation:     Qt.Horizontal

            Frame {
                SplitView.preferredWidth: 340
                SplitView.minimumWidth:   240

                background: Rectangle {
                    color: "#282828"
                    radius: 8
                    border.color: "#504945"
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 4

                    Label {
                        text: "Results"
                        font.bold: true
                        font.pixelSize: 11
                        color: "#a89984"
                    }

                    ListView {
                        id: resultsList
                        Layout.fillWidth:  true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 2
                        model: matchController.searchResults

                        delegate: ItemDelegate {
                            id: resultRow
                            required property var modelData

                            width:  ListView.view.width
                            padding: 6
                            highlighted: matchController.selectedSearchIndex === modelData.index

                            contentItem: GridLayout {
                                columns: 4
                                columnSpacing: 6
                                rowSpacing: 2
                                width: parent.width

                                Label {
                                    Layout.columnSpan: 4
                                    Layout.fillWidth: true
                                    text: modelData.title
                                    elide: Text.ElideRight
                                    font.pixelSize: 12
                                    color: "#ebdbb2"
                                    font.bold: resultRow.highlighted
                                }
                                Label {
                                    text: modelData.releaseYear > 0 ? modelData.releaseYear : "—"
                                    font.pixelSize: 10
                                    color: "#928374"
                                }
                                Label {
                                    text: modelData.provider || "—"
                                    font.pixelSize: 10
                                    color: "#928374"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: modelData.confidence > 0 ? modelData.confidence + "%" : "—"
                                    font.pixelSize: 10
                                    color: "#b8bb26"
                                    horizontalAlignment: Text.AlignRight
                                    Layout.fillWidth: true
                                }
                            }

                            onClicked: matchController.selectSearchResult(modelData.index)

                            background: Rectangle {
                                color: parent.highlighted ? "#3f4d4f"
                                     : parent.hovered     ? "#383838"
                                     : "transparent"
                                radius: 4
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: 10
                        color: "#928374"
                        text: matchController.searchStatus
                    }
                }
            }

            Frame {
                SplitView.fillWidth: true

                background: Rectangle {
                    color: "#282828"
                    radius: 8
                    border.color: "#504945"
                }

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: parent.width
                        spacing: 8

                        Label {
                            text: "Preview"
                            font.bold: true
                            font.pixelSize: 11
                            color: "#a89984"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 160
                            color: "#1d2021"
                            radius: 6

                            Image {
                                anchors.fill: parent
                                anchors.margins: 4
                                source: matchController.previewMetadata.boxArtUrl || ""
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                visible: (matchController.previewMetadata.boxArtUrl || "").length > 0
                            }

                            Label {
                                anchors.centerIn: parent
                                text: "No preview artwork"
                                color: "#665c54"
                                font.pixelSize: 11
                                visible: (matchController.previewMetadata.boxArtUrl || "").length === 0
                            }
                        }

                        PreviewField {
                            label: "Title"
                            value: matchController.previewMetadata.title || ""
                            bold:  true
                        }
                        PreviewField {
                            label: "Publisher"
                            value: matchController.previewMetadata.publisher || ""
                        }
                        PreviewField {
                            label: "Developer"
                            value: matchController.previewMetadata.developer || ""
                        }
                        PreviewField {
                            label: "Release"
                            value: matchController.previewMetadata.releaseYear
                                   ? matchController.previewMetadata.releaseYear.toString()
                                   : matchController.previewMetadata.releaseDate || ""
                        }
                        PreviewField {
                            label: "Genre"
                            value: matchController.previewMetadata.genre || ""
                        }
                        PreviewField {
                            label: "Rating"
                            value: {
                                const r = matchController.previewMetadata.rating
                                return (r && r > 0) ? r.toFixed(1) + " / 10" : ""
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Plot"
                            color: "#a89984"
                            font.pixelSize: 10
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 11
                            color: "#ebdbb2"
                            maximumLineCount: 8
                            elide: Text.ElideRight
                            text: matchController.previewMetadata.description || "—"
                        }
                    }
                }
            }
        }

        Label {
            text: "Import fields"
            font.bold: true
            font.pixelSize: 11
            color: "#a89984"
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6

            CheckBox { id: importTitle;       text: "Title";       checked: true;  font.pixelSize: 11 }
            CheckBox { id: importPlot;        text: "Plot";        checked: true;  font.pixelSize: 11 }
            CheckBox { id: importPublisher;   text: "Publisher";   checked: true;  font.pixelSize: 11 }
            CheckBox { id: importDeveloper;   text: "Developer";   checked: true;  font.pixelSize: 11 }
            CheckBox { id: importGenre;        text: "Genre";       checked: true;  font.pixelSize: 11 }
            CheckBox { id: importRelease;      text: "Release";     checked: true;  font.pixelSize: 11 }
            CheckBox { id: importRating;       text: "Rating";      checked: false; font.pixelSize: 11 }
            CheckBox { id: importArtwork;      text: "Box art";     checked: true;  font.pixelSize: 11 }
        }

        CheckBox {
            id: skipOverwrite
            text: "Do not overwrite existing data"
            font.pixelSize: 11
        }
    }

    Connections {
        target: matchController
        function onSearchQueryChanged() {
            queryField.text = matchController.searchQuery
        }
        function onSearchProviderChanged() {
            const idx = providerCombo.model.indexOf(matchController.searchProvider)
            if (idx >= 0)
                providerCombo.currentIndex = idx
        }
    }

    footer: RowLayout {
        spacing: 8
        anchors.margins: 8

        Item { Layout.fillWidth: true }

        Button {
            text: "Cancel"
            onClicked: dialog.close()
        }
        Button {
            text: "Apply match"
            highlighted: true
            enabled: matchController.selectedSearchIndex >= 0 && !matchController.searching
            onClicked: applyMatch()
        }
    }

    function runSearch() {
        matchController.runSearch(providerCombo.currentText, queryField.text)
    }

    function applyMatch() {
        const ok = matchController.applySearchMatch(
            true,
            importArtwork.checked,
            skipOverwrite.checked,
            importTitle.checked,
            importPlot.checked,
            importPublisher.checked,
            importDeveloper.checked,
            importGenre.checked,
            importRelease.checked,
            importRating.checked)
        if (!ok)
            return
        if (importArtwork.checked)
            artworkController.downloadSelected()
        dialog.close()
    }

    component PreviewField: RowLayout {
        property string label: ""
        property string value: ""
        property bool bold: false

        Layout.fillWidth: true
        spacing: 6

        Label {
            text: label + ":"
            color: "#a89984"
            font.pixelSize: 10
            font.bold: true
            Layout.minimumWidth: 68
        }
        Label {
            Layout.fillWidth: true
            text: value.length > 0 ? value : "—"
            color: value.length > 0 ? "#ebdbb2" : "#504945"
            font.pixelSize: 11
            font.bold: bold
            wrapMode: Text.WordWrap
        }
    }
}
