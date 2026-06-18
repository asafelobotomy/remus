import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

// Right-side inspector — TMM-style tabbed detail panel for the selected ROM.
Frame {
    id: panel

    signal matchSearchRequested()

    Layout.fillHeight: true

    property int currentTab: 0

    background: Rectangle {
        color:        Theme.panelBg
        border.color: Theme.panelBorder
        radius:       Theme.panelRadius
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
            color:               Theme.textMuted
            font.pixelSize:      Theme.fontLg
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
                    font.pixelSize:   Theme.fontTitle
                    font.bold:        true
                    color:            Theme.textPrimary
                    elide:            Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    visible:          (appController.selectedFileData.discSetMemberCount || 0) > 1
                                   || (appController.selectedFileData.catalogDiscCount || 0) > 1
                    text: {
                        const n = appController.selectedFileData.discNumber || 0
                        const owned = appController.selectedFileData.discSetMemberCount || 0
                        const catalog = appController.selectedFileData.catalogDiscCount || 0
                        if (catalog > 1) {
                            const progress = owned + "/" + catalog + " discs"
                            return n > 0 ? ("Disc " + n + " · " + progress) : progress
                        }
                        return n > 0 ? ("Disc " + n + " of " + owned) : (owned + " discs")
                    }
                    font.pixelSize:   Theme.fontSm
                    color:            appController.selectedFileData.discSetComplete === false
                                      ? Theme.warn : Theme.textDim
                    elide:            Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text:             appController.selectedFileData.systemName || ""
                    font.pixelSize:   Theme.fontSm
                    color:            Theme.textDim
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
                color:            Theme.border
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

                        LabelField {
                            label: "Title"
                            value: appController.selectedMatchData.title || ""
                            bold:  true
                        }
                        LabelField {
                            label: "Platform"
                            value: appController.selectedFileData.systemName || ""
                        }
                        LabelField {
                            label: "Release"
                            value: {
                                const y = appController.selectedMatchData.releaseYear
                                return (y && y > 0) ? y.toString() : ""
                            }
                        }
                        LabelField {
                            label: "Genre"
                            value: appController.selectedMatchData.genre || ""
                        }
                        LabelField {
                            label: "Publisher"
                            value: appController.selectedMatchData.publisher || ""
                        }
                        LabelField {
                            label: "Developer"
                            value: appController.selectedMatchData.developer || ""
                        }
                        LabelField {
                            label: "Rating"
                            value: {
                                const r = appController.selectedMatchData.rating
                                return (r && r > 0) ? r.toFixed(1) + " / 10" : ""
                            }
                        }
                        LabelField {
                            label: "Region"
                            value: appController.selectedMatchData.region || ""
                        }
                        LabelField {
                            label: "Format"
                            value: appController.selectedFileData.extension || ""
                        }

                        SectionDivider {}

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing:          3

                            Label {
                                text:           "Description"
                                color:          Theme.textMuted
                                font.pixelSize: Theme.fontXs
                                font.bold:      true
                            }
                            Label {
                                Layout.fillWidth: true
                                text:     appController.selectedMatchData.description || ""
                                wrapMode: Text.WordWrap
                                color:    appController.selectedMatchData.description
                                          ? Theme.textBody : Theme.borderSub
                                font.pixelSize: Theme.fontSm
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
                            color:          Theme.textMuted
                            font.pixelSize: Theme.fontXs
                            font.bold:      true
                        }

                        Label {
                            Layout.fillWidth: true
                            visible:          (appController.selectedFileData.discSetMemberCount || 0) > 1
                                           || (appController.selectedFileData.catalogDiscCount || 0) > 1
                            text: {
                                const owned = appController.selectedFileData.discSetMemberCount || 0
                                const catalog = appController.selectedFileData.catalogDiscCount || 0
                                if (catalog > 1)
                                    return "Disc set (" + owned + "/" + catalog + " catalog discs)"
                                return "Disc set (" + owned + " discs)"
                            }
                            color:            appController.selectedFileData.discSetComplete === false
                                              ? Theme.warn : Theme.textMuted
                            font.pixelSize:   Theme.fontXs
                            font.bold:        true
                        }

                        Repeater {
                            model: (appController.selectedFileData.discSetMembers || [])
                            delegate: LabelField {
                                Layout.fillWidth: true
                                visible:          (appController.selectedFileData.discSetMemberCount || 0) > 1
                                label:            modelData.discLabel || ("Disc " + modelData.discNumber)
                                value:            modelData.path || ""
                                mono:             true
                            }
                        }

                        SectionDivider {
                            visible: (appController.selectedFileData.discSetMemberCount || 0) > 1
                        }

                        LabelField {
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
                        LabelField {
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
                        LabelField {
                            visible: (appController.selectedFileData.isBundled    || false) &&
                                     !(appController.selectedFileData.isOrganized || false) &&
                                     (appController.selectedFileData.bundleOutputPath || "").length > 0
                            label:   "Bundle"
                            value:   appController.selectedFileData.bundleOutputPath || ""
                            mono:    true
                        }
                        LabelField {
                            visible: (appController.selectedFileData.isOrganized  || false) &&
                                     (appController.selectedFileData.organizedPath || "").length > 0
                            label:   "Organized"
                            value:   appController.selectedFileData.organizedPath || ""
                            mono:    true
                        }

                        SectionDivider {}

                        Label {
                            text:           "Hashes"
                            color:          Theme.textMuted
                            font.pixelSize: Theme.fontXs
                            font.bold:      true
                        }
                        LabelField { label: "MD5";  value: appController.selectedFileData.md5  || ""; mono: true }
                        LabelField { label: "SHA1"; value: appController.selectedFileData.sha1 || ""; mono: true }
                        LabelField { label: "CRC";  value: appController.selectedFileData.crc32 || ""; mono: true }
                        LabelField {
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
                                font.pixelSize: Theme.fontSm
                                enabled:   appController.selectedFileId > 0
                                onClicked: artworkController.refreshSelectedArtwork()
                            }
                            Button {
                                text:      "Download"
                                flat:      true
                                font.pixelSize: Theme.fontSm
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
                            color:                 Theme.surface
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
                                color:            Theme.textDisabled
                                font.pixelSize:   Theme.fontMd
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
                                    color:            Theme.border
                                    font.pixelSize:   Theme.fontXs
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
                                    color:            Theme.border
                                    font.pixelSize:   Theme.fontXs
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode:         Text.WordWrap
                            font.pixelSize:   Theme.fontXs
                            color:            Theme.textDim
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
                            font.pixelSize: Theme.fontMd
                            color:          appController.selectedMatchData.confirmed ? Theme.success
                                            : appController.selectedMatchData.rejected ? Theme.error
                                            : appController.selectedMatchData.matchId ? Theme.warn
                                            : Theme.textMuted
                        }

                        LabelField {
                            label: "Method"
                            value: appController.selectedMatchData.method || ""
                            visible: !!appController.selectedMatchData.matchId
                        }
                        LabelField {
                            label: "Confidence"
                            value: {
                                const c = appController.selectedMatchData.confidence
                                return (c && c > 0) ? Math.round(c) + "%" : ""
                            }
                            visible: !!appController.selectedMatchData.matchId
                        }
                        LabelField {
                            label: "Matched title"
                            value: appController.selectedMatchData.title || ""
                            visible: !!appController.selectedMatchData.matchId
                        }

                        SectionDivider {}

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                text:      "Search && match\u2026"
                                enabled:   appController.selectedFileId > 0
                                font.pixelSize: Theme.fontSm
                                highlighted: true
                                onClicked: panel.matchSearchRequested()
                            }
                            Button {
                                text:      "Hash && Match"
                                enabled:   !hashController.hashing &&
                                           !matchController.matching &&
                                           appController.selectedFileId > 0
                                font.pixelSize: Theme.fontSm
                                onClicked: workflowController.hashAndMatchSelected()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Button {
                                text:      "\u2713 Confirm"
                                enabled:   appController.selectedFileId > 0 &&
                                           !appController.selectedMatchData.confirmed &&
                                           !!appController.selectedMatchData.matchId
                                font.pixelSize: Theme.fontSm
                                padding:   6
                                onClicked: matchController.confirmSelected()
                            }
                            Button {
                                text:      "\u2717 Reject"
                                enabled:   appController.selectedFileId > 0 &&
                                           !appController.selectedMatchData.rejected &&
                                           !!appController.selectedMatchData.matchId
                                font.pixelSize: Theme.fontSm
                                padding:   6
                                onClicked: matchController.rejectSelected()
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
                            color:            Theme.accentAlt
                            font.pixelSize:   Theme.fontXs
                            wrapMode:         Text.WordWrap
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
        Layout.fillWidth:    true
        height:              1
        color:               Theme.borderSub
        Layout.topMargin:    4
        Layout.bottomMargin: 4
    }
}
