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
            font.pixelSize: Theme.fontSm
            color: Theme.textDim
            text: matchController.searchRomPath.length > 0
                  ? matchController.searchRomPath
                  : "No ROM selected"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Provider"
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
            }
            ComboBox {
                id: providerCombo
                Layout.preferredWidth: 160
                model: matchController.enabledProviders()
                font.pixelSize: Theme.fontMd
                Component.onCompleted: {
                    const idx = model.indexOf(matchController.searchProvider)
                    if (idx >= 0)
                        currentIndex = idx
                }
                onActivated: matchController.searchProvider = model[currentIndex]
            }

            Label {
                text: "System"
                color: Theme.textMuted
                font.pixelSize: Theme.fontSm
            }
            Label {
                Layout.preferredWidth: 120
                elide: Text.ElideRight
                text: matchController.searchSystem || "\u2014"
                color: Theme.textBody
                font.pixelSize: Theme.fontSm
            }

            TextField {
                id: queryField
                Layout.fillWidth: true
                placeholderText: "Search query\u2026"
                text: matchController.searchQuery
                font.pixelSize: Theme.fontMd
                onAccepted: runSearch()
            }

            Button {
                text: matchController.searching ? "Searching\u2026" : "Search"
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
                    color: Theme.surface
                    radius: 8
                    border.color: Theme.border
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 4

                    Label {
                        text: "Results"
                        font.bold: true
                        font.pixelSize: Theme.fontSm
                        color: Theme.textMuted
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
                                    font.pixelSize: Theme.fontMd
                                    color: Theme.textBody
                                    font.bold: resultRow.highlighted
                                }
                                Label {
                                    text: modelData.releaseYear > 0 ? modelData.releaseYear : "\u2014"
                                    font.pixelSize: Theme.fontXs
                                    color: Theme.textDim
                                }
                                Label {
                                    text: modelData.provider || "\u2014"
                                    font.pixelSize: Theme.fontXs
                                    color: Theme.textDim
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: modelData.confidence > 0 ? modelData.confidence + "%" : "\u2014"
                                    font.pixelSize: Theme.fontXs
                                    color: Theme.success
                                    horizontalAlignment: Text.AlignRight
                                    Layout.fillWidth: true
                                }
                            }

                            onClicked: matchController.selectSearchResult(modelData.index)

                            background: Rectangle {
                                color: parent.highlighted ? Theme.selected
                                     : parent.hovered     ? Theme.hover
                                     : "transparent"
                                radius: 4
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontXs
                        color: Theme.textDim
                        text: matchController.searchStatus
                    }
                }
            }

            Frame {
                SplitView.fillWidth: true

                background: Rectangle {
                    color: Theme.surface
                    radius: 8
                    border.color: Theme.border
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
                            font.pixelSize: Theme.fontSm
                            color: Theme.textMuted
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 160
                            color: Theme.background
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
                                color: Theme.textDisabled
                                font.pixelSize: Theme.fontSm
                                visible: (matchController.previewMetadata.boxArtUrl || "").length === 0
                            }
                        }

                        LabelField {
                            label: "Title"
                            value: matchController.previewMetadata.title || ""
                            bold:  true
                            minWidth: 68
                        }
                        LabelField {
                            label: "Publisher"
                            value: matchController.previewMetadata.publisher || ""
                            minWidth: 68
                        }
                        LabelField {
                            label: "Developer"
                            value: matchController.previewMetadata.developer || ""
                            minWidth: 68
                        }
                        LabelField {
                            label: "Release"
                            value: matchController.previewMetadata.releaseYear
                                   ? matchController.previewMetadata.releaseYear.toString()
                                   : matchController.previewMetadata.releaseDate || ""
                            minWidth: 68
                        }
                        LabelField {
                            label: "Genre"
                            value: matchController.previewMetadata.genre || ""
                            minWidth: 68
                        }
                        LabelField {
                            label: "Rating"
                            value: {
                                const r = matchController.previewMetadata.rating
                                return (r && r > 0) ? r.toFixed(1) + " / 10" : ""
                            }
                            minWidth: 68
                        }

                        Label {
                            Layout.fillWidth: true
                            text: "Plot"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontXs
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: Theme.fontSm
                            color: Theme.textBody
                            maximumLineCount: 8
                            elide: Text.ElideRight
                            text: matchController.previewMetadata.description || "\u2014"
                        }
                    }
                }
            }
        }

        Label {
            text: "Import fields"
            font.bold: true
            font.pixelSize: Theme.fontSm
            color: Theme.textMuted
        }

        Flow {
            Layout.fillWidth: true
            spacing: 6

            CheckBox { id: importTitle;       text: "Title";       checked: true;  font.pixelSize: Theme.fontSm }
            CheckBox { id: importPlot;        text: "Plot";        checked: true;  font.pixelSize: Theme.fontSm }
            CheckBox { id: importPublisher;   text: "Publisher";   checked: true;  font.pixelSize: Theme.fontSm }
            CheckBox { id: importDeveloper;   text: "Developer";   checked: true;  font.pixelSize: Theme.fontSm }
            CheckBox { id: importGenre;        text: "Genre";       checked: true;  font.pixelSize: Theme.fontSm }
            CheckBox { id: importRelease;      text: "Release";     checked: true;  font.pixelSize: Theme.fontSm }
            CheckBox { id: importRating;       text: "Rating";      checked: false; font.pixelSize: Theme.fontSm }
            CheckBox { id: importArtwork;      text: "Box art";     checked: true;  font.pixelSize: Theme.fontSm }
        }

        CheckBox {
            id: skipOverwrite
            text: "Do not overwrite existing data"
            font.pixelSize: Theme.fontSm
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
}
