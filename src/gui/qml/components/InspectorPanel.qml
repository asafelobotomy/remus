import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

// Right-side inspector — TMM-style tabbed detail panel for the selected ROM.
Frame {
    id: panel

    Layout.fillHeight: true

    property int currentTab: 0

    background: Rectangle {
        color:        "#1d2021"
        border.color: "#504945"
        radius:       12
    }

    ColumnLayout {
        anchors.fill:    parent
        anchors.margins: 0
        spacing:         0

        Label {
            visible:             appController.selectedFileId <= 0
            Layout.fillWidth:    true
            Layout.fillHeight:   true
            text:                "Select a ROM from the library to view its details."
            wrapMode:            Text.WordWrap
            color:               "#a89984"
            font.pixelSize:      13
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment:   Text.AlignVCenter
        }

        ColumnLayout {
            visible:          appController.selectedFileId > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing:          0

            // ── Header ──────────────────────────────────────────────────────
            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: 10
                spacing:        2

                Label {
                    Layout.fillWidth: true
                    text:             appController.selectedMatchData.title
                          || appController.selectedFileData.baseTitle
                          || appController.selectedFileData.filename
                          || "Selected ROM"
                    font.pixelSize:   15
                    font.bold:        true
                    color:            "#fbf1c7"
                    elide:            Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text:             appController.selectedFileData.systemName || ""
                    font.pixelSize:   11
                    color:            "#928374"
                    elide:            Text.ElideRight
                    visible:          (appController.selectedFileData.systemName || "").length > 0
                }
            }

            TabBar {
                id: tabBar
                Layout.fillWidth: true
                currentIndex: panel.currentTab
                onCurrentIndexChanged: panel.currentTab = currentIndex

                TabButton { text: "Details" }
                TabButton { text: "Files" }
                TabButton { text: "Artwork" }
                TabButton { text: "Match" }
            }

            Rectangle {
                Layout.fillWidth: true
                height:           1
                color:            "#504945"
            }

            StackLayout {
                Layout.fillWidth:  true
                Layout.fillHeight: true
                currentIndex:      tabBar.currentIndex

                // ── Details ─────────────────────────────────────────────────
                ScrollView {
                    clip: true
                    contentWidth:  availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width:            parent.width
                        Layout.margins:   10
                        spacing:          6

                        MetaField {
                            label: "Title"
                            value: appController.selectedMatchData.title || ""
                            bold:  true
                        }
                        MetaField {
                            label: "Platform"
                            value: appController.selectedFileData.systemName || ""
                        }
                        MetaField {
                            label: "Release"
                            value: {
                                const y = appController.selectedMatchData.releaseYear
                                return (y && y > 0) ? y.toString() : ""
                            }
                        }
                        MetaField {
                            label: "Genre"
                            value: appController.selectedMatchData.genre || ""
                        }
                        MetaField {
                            label: "Publisher"
                            value: appController.selectedMatchData.publisher || ""
                        }
                        MetaField {
                            label: "Developer"
                            value: appController.selectedMatchData.developer || ""
                        }
                        MetaField {
                            label: "Rating"
                            value: {
                                const r = appController.selectedMatchData.rating
                                return (r && r > 0) ? r.toFixed(1) + " / 10" : ""
                            }
                        }
                        MetaField {
                            label: "Region"
                            value: appController.selectedMatchData.region || ""
                        }
                        MetaField {
                            label: "Format"
                            value: appController.selectedFileData.extension || ""
                        }

                        panel.SectionDivider {}

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing:          3

                            Label {
                                text:           "Description"
                                color:          "#a89984"
                                font.pixelSize: 10
                                font.bold:      true
                            }
                            Label {
                                Layout.fillWidth: true
                                text:     appController.selectedMatchData.description || ""
                                wrapMode: Text.WordWrap
                                color:    appController.selectedMatchData.description
                                          ? "#ebdbb2" : "#504945"
                                font.pixelSize: 11
                            }
                        }

                        Item { Layout.preferredHeight: 12 }
                    }
                }

                // ── Files ───────────────────────────────────────────────────
                ScrollView {
                    clip: true
                    contentWidth:  availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width:          parent.width
                        Layout.margins: 10
                        spacing:        6

                        Label {
                            text:           "Paths"
                            color:          "#a89984"
                            font.pixelSize: 10
                            font.bold:      true
                        }

                        MetaField {
                            property bool   _origExists: appController.selectedFileData.originalExists || false
                            property bool   _currExists: appController.selectedFileData.currentExists  || false
                            property string _orig: appController.selectedFileData.originalPath || ""
                            property string _curr: appController.selectedFileData.path         || ""

                            visible: _origExists || (_currExists && _curr !== _orig &&
                                     !(appController.selectedFileData.isOrganized || false))
                            label:   "ROM Path"
                            value:   _origExists ? _orig : _curr
                            mono:    true
                        }
                        MetaField {
                            property string _curr: appController.selectedFileData.path         || ""
                            property string _orig: appController.selectedFileData.originalPath || ""

                            visible: (appController.selectedFileData.isConverted  || false) &&
                                     !(appController.selectedFileData.isBundled   || false) &&
                                     !(appController.selectedFileData.isOrganized || false) &&
                                     _curr.length > 0 && _curr !== _orig
                            label:   "Converted ROM"
                            value:   _curr
                            mono:    true
                        }
                        MetaField {
                            visible: (appController.selectedFileData.isBundled    || false) &&
                                     !(appController.selectedFileData.isOrganized || false) &&
                                     (appController.selectedFileData.bundleOutputPath || "").length > 0
                            label:   "Bundle"
                            value:   appController.selectedFileData.bundleOutputPath || ""
                            mono:    true
                        }
                        MetaField {
                            visible: (appController.selectedFileData.isOrganized  || false) &&
                                     (appController.selectedFileData.organizedPath || "").length > 0
                            label:   "Organized"
                            value:   appController.selectedFileData.organizedPath || ""
                            mono:    true
                        }

                        panel.SectionDivider {}

                        Label {
                            text:           "Hashes"
                            color:          "#a89984"
                            font.pixelSize: 10
                            font.bold:      true
                        }
                        MetaField { label: "MD5";  value: appController.selectedFileData.md5  || ""; mono: true }
                        MetaField { label: "SHA1"; value: appController.selectedFileData.sha1 || ""; mono: true }
                        MetaField { label: "CRC";  value: appController.selectedFileData.crc32 || ""; mono: true }
                        MetaField {
                            label: "Size"
                            value: panel.formatFileSize(appController.selectedFileData.fileSize)
                        }

                        Item { Layout.preferredHeight: 12 }
                    }
                }

                // ── Artwork ─────────────────────────────────────────────────
                ScrollView {
                    clip: true
                    contentWidth:  availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width:          parent.width
                        Layout.margins: 10
                        spacing:        8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                text:      "Refresh"
                                flat:      true
                                font.pixelSize: 11
                                enabled:   appController.selectedFileId > 0
                                onClicked: artworkController.refreshSelectedArtwork()
                            }
                            Button {
                                text:      "Download"
                                flat:      true
                                font.pixelSize: 11
                                enabled:   appController.selectedFileId > 0 &&
                                           !artworkController.downloading
                                onClicked: artworkController.downloadSelected()
                            }
                            Item { Layout.fillWidth: true }
                        }

                        ProgressCard {
                            Layout.fillWidth: true
                            visible:          artworkController.downloading ||
                                              artworkController.lastError.length > 0 ||
                                              artworkController.localArtworkPath.length > 0
                            title:            "Artwork"
                            progressValue:    artworkController.downloadProgress
                            progressTotal:    artworkController.downloadTotal
                            message:          artworkController.lastError.length > 0
                                              ? artworkController.lastError
                                              : artworkController.localArtworkPath
                        }

                        Rectangle {
                            Layout.fillWidth:      true
                            Layout.preferredHeight: 200
                            color:                 "#282828"
                            radius:                8

                            Image {
                                anchors.fill:    parent
                                anchors.margins: 4
                                source:          artworkController.previewUrl
                                fillMode:        Image.PreserveAspectFit
                                clip:            true
                                cache:           false
                                asynchronous:    true
                                visible:         artworkController.previewUrl.toString().length > 0 &&
                                                 status !== Image.Error
                            }

                            Label {
                                anchors.centerIn: parent
                                text:             "Box Art"
                                color:            "#665c54"
                                font.pixelSize:   12
                                visible:          artworkController.previewUrl.toString().length === 0
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing:          4

                            Rectangle {
                                Layout.fillWidth:      true
                                Layout.preferredHeight: 72
                                color:                 "#242424"
                                radius:                6
                                Label {
                                    anchors.centerIn: parent
                                    text:             "Title Screen"
                                    color:            "#504945"
                                    font.pixelSize:   10
                                }
                            }
                            Rectangle {
                                Layout.fillWidth:      true
                                Layout.preferredHeight: 72
                                color:                 "#242424"
                                radius:                6
                                Label {
                                    anchors.centerIn: parent
                                    text:             "Gameplay"
                                    color:            "#504945"
                                    font.pixelSize:   10
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode:         Text.WordWrap
                            font.pixelSize:   10
                            color:            "#928374"
                            text:             artworkController.previewUrl.toString().length > 0
                                              ? artworkController.previewUrl.toString()
                                              : "No artwork cached. Download after confirming a match."
                        }

                        Item { Layout.preferredHeight: 8 }
                    }
                }

                // ── Match ───────────────────────────────────────────────────
                ScrollView {
                    clip: true
                    contentWidth:  availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width:          parent.width
                        Layout.margins: 10
                        spacing:        8

                        Label {
                            text:           appController.selectedMatchData.confirmed
                                            ? "Match confirmed"
                                            : appController.selectedMatchData.rejected
                                              ? "Match rejected"
                                              : appController.selectedMatchData.matchId
                                                ? "Match pending review"
                                                : "No match yet"
                            font.bold:      true
                            font.pixelSize: 12
                            color:          appController.selectedMatchData.confirmed ? "#b8bb26"
                                            : appController.selectedMatchData.rejected ? "#fb4934"
                                            : appController.selectedMatchData.matchId ? "#fabd2f"
                                            : "#a89984"
                        }

                        MetaField {
                            label: "Method"
                            value: appController.selectedMatchData.method || ""
                            visible: !!appController.selectedMatchData.matchId
                        }
                        MetaField {
                            label: "Confidence"
                            value: {
                                const c = appController.selectedMatchData.confidence
                                return (c && c > 0) ? Math.round(c) + "%" : ""
                            }
                            visible: !!appController.selectedMatchData.matchId
                        }
                        MetaField {
                            label: "Matched title"
                            value: appController.selectedMatchData.title || ""
                            visible: !!appController.selectedMatchData.matchId
                        }

                        panel.SectionDivider {}

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                text:      "\u2713 Confirm"
                                enabled:   appController.selectedFileId > 0 &&
                                           !appController.selectedMatchData.confirmed &&
                                           !!appController.selectedMatchData.matchId
                                font.pixelSize: 11
                                padding:   6
                                onClicked: matchController.confirmSelected()
                            }
                            Button {
                                text:      "\u2717 Reject"
                                enabled:   appController.selectedFileId > 0 &&
                                           !appController.selectedMatchData.rejected &&
                                           !!appController.selectedMatchData.matchId
                                font.pixelSize: 11
                                padding:   6
                                onClicked: matchController.rejectSelected()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                text:      "Hash && Match"
                                enabled:   !hashController.hashing &&
                                           !matchController.matching &&
                                           appController.selectedFileId > 0
                                font.pixelSize: 11
                                onClicked: workflowController.hashAndMatchSelected()
                            }
                            Button {
                                text:      "Re-match"
                                enabled:   !matchController.matching &&
                                           appController.selectedFileId > 0
                                font.pixelSize: 11
                                flat:      true
                                onClicked: matchController.matchSelected()
                            }
                        }

                        ProgressCard {
                            Layout.fillWidth: true
                            visible:       hashController.hashing || matchController.matching
                            title:         "Match Progress"
                            progressValue: hashController.hashing
                                               ? hashController.hashedFiles
                                               : matchController.matchedFiles
                            progressTotal: hashController.hashing
                                               ? hashController.totalFiles
                                               : matchController.totalMatchFiles
                            message:       hashController.hashing
                                               ? hashController.progressMessage
                                               : matchController.progressMessage
                        }

                        Label {
                            Layout.fillWidth: true
                            visible:          matchController.lastMessage.length > 0
                            text:             matchController.lastMessage
                            color:            "#83a598"
                            font.pixelSize:   10
                            wrapMode:         Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode:         Text.WordWrap
                            font.pixelSize:   10
                            color:            "#665c54"
                            text:             "A searchable match picker dialog is planned for a later release."
                        }

                        Item { Layout.preferredHeight: 8 }
                    }
                }
            }
        }
    }

    function formatFileSize(bytes) {
        if (!bytes || bytes <= 0)
            return ""
        if (bytes >= 1073741824)
            return (bytes / 1073741824).toFixed(2) + " GB"
        if (bytes >= 1048576)
            return (bytes / 1048576).toFixed(2) + " MB"
        return (bytes / 1024).toFixed(1) + " KB"
    }

    component SectionDivider: Rectangle {
        Layout.fillWidth: true
        height:           1
        color:            "#3c3836"
        Layout.topMargin: 4
        Layout.bottomMargin: 4
    }

    component MetaField: RowLayout {
        property string label: ""
        property string value: ""
        property bool   bold:  false
        property bool   mono:  false

        Layout.fillWidth: true
        spacing:          6

        Label {
            text:           label + ":"
            color:          "#a89984"
            font.pixelSize: 10
            font.bold:      true
            Layout.minimumWidth: 72
        }
        Label {
            Layout.fillWidth: true
            text:             value.length > 0 ? value : "—"
            color:            value.length > 0 ? "#ebdbb2" : "#504945"
            font.pixelSize:   11
            font.bold:        bold
            font.family:      mono ? "monospace" : font.family
            elide:            mono ? Text.ElideMiddle : Text.ElideRight
            wrapMode:         mono ? Text.NoWrap : Text.WordWrap
        }
    }
}
