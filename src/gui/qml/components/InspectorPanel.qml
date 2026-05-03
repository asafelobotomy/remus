import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Right-side inspector: shows rich metadata for the selected ROM.
// All fields remain visible but empty until the ROM has a confirmed match.
Frame {
    id: panel

    Layout.fillHeight: true

    background: Rectangle {
        color:        "#1d2021"
        border.color: "#504945"
        radius:       12
    }

    ScrollView {
        anchors.fill:            parent
        contentWidth:            availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width:   parent.width
            spacing: 0

            // ── No-selection hint ───────────────────────────────────────────
            Label {
                visible:             appController.selectedFileId <= 0
                Layout.fillWidth:    true
                text:                "Select a ROM from the queue to view its details."
                wrapMode:            Text.WordWrap
                color:               "#a89984"
                font.pixelSize:      13
                horizontalAlignment: Text.AlignHCenter
                topPadding:          40
            }

            // ── Content ─────────────────────────────────────────────────────
            ColumnLayout {
                visible:          appController.selectedFileId > 0
                Layout.fillWidth: true
                spacing:          0

                // ── Box art ─────────────────────────────────────────────────
                Rectangle {
                    Layout.fillWidth:      true
                    Layout.preferredHeight: 180
                    color:                 "#282828"

                    Image {
                        anchors.fill:    parent
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
                        text:             artworkController.previewUrl.toString().length > 0
                                          ? "" : "Box Art"
                        color:            "#665c54"
                        font.pixelSize:   12
                        visible:          artworkController.previewUrl.toString().length === 0
                    }
                }

                // ── Screenshots row ──────────────────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    spacing:          2

                    Rectangle {
                        Layout.fillWidth:      true
                        Layout.preferredHeight: 80
                        color:                 "#242424"
                        Label {
                            anchors.centerIn: parent
                            text:             "Title Screen"
                            color:            "#504945"
                            font.pixelSize:   10
                        }
                    }
                    Rectangle {
                        Layout.fillWidth:      true
                        Layout.preferredHeight: 80
                        color:                 "#242424"
                        Label {
                            anchors.centerIn: parent
                            text:             "Gameplay"
                            color:            "#504945"
                            font.pixelSize:   10
                        }
                    }
                }

                // ── Metadata fields ──────────────────────────────────────────
                ColumnLayout {
                    Layout.fillWidth:  true
                    Layout.topMargin:  10
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10
                    spacing:           6

                    // Title
                    MetaField {
                        label: "Title"
                        value: appController.selectedMatchData.title || ""
                        bold:  true
                    }

                    // Platform / System
                    MetaField {
                        label: "Platform"
                        value: appController.selectedFileData.systemName || ""
                    }

                    // Release Date
                    MetaField {
                        label: "Release"
                        value: {
                            const y = appController.selectedMatchData.releaseYear
                            return (y && y > 0) ? y.toString() : ""
                        }
                    }

                    // Genre
                    MetaField {
                        label: "Genre"
                        value: appController.selectedMatchData.genre || ""
                    }

                    // Publisher
                    MetaField {
                        label: "Publisher"
                        value: appController.selectedMatchData.publisher || ""
                    }

                    // Developer
                    MetaField {
                        label: "Developer"
                        value: appController.selectedMatchData.developer || ""
                    }

                    // Rating
                    MetaField {
                        label: "Rating"
                        value: {
                            const r = appController.selectedMatchData.rating
                            return (r && r > 0) ? r.toFixed(1) + " / 10" : ""
                        }
                    }

                    // Region
                    MetaField {
                        label: "Region"
                        value: appController.selectedMatchData.region || ""
                    }

                    // Format
                    MetaField {
                        label: "Format"
                        value: appController.selectedFileData.extension || ""
                    }

                    // Divider
                    Rectangle {
                        Layout.fillWidth: true
                        height:           1
                        color:            "#3c3836"
                        Layout.topMargin: 2
                        Layout.bottomMargin: 2
                    }

                    // Description
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
                            visible:  true
                        }
                    }

                    // Divider
                    Rectangle {
                        Layout.fillWidth: true
                        height:           1
                        color:            "#3c3836"
                        Layout.topMargin: 2
                        Layout.bottomMargin: 2
                    }

                    // Hash info
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing:          3

                        Label {
                            text:           "Hash Info"
                            color:          "#a89984"
                            font.pixelSize: 10
                            font.bold:      true
                        }
                        MetaField { label: "MD5";  value: appController.selectedFileData.md5  || ""; mono: true }
                        MetaField { label: "SHA1"; value: appController.selectedFileData.sha1 || ""; mono: true }
                        MetaField { label: "CRC";  value: appController.selectedFileData.crc32 || ""; mono: true }
                        MetaField {
                            label: "Size"
                            value: {
                                const b = appController.selectedFileData.fileSize
                                if (!b || b <= 0) return ""
                                if (b >= 1073741824) return (b / 1073741824).toFixed(2) + " GB"
                                if (b >= 1048576)    return (b / 1048576).toFixed(2) + " MB"
                                return (b / 1024).toFixed(1) + " KB"
                            }
                        }
                    }

                    // Divider
                    Rectangle {
                        Layout.fillWidth: true
                        height:           1
                        color:            "#3c3836"
                        Layout.topMargin: 2
                        Layout.bottomMargin: 2
                    }

                    // Match info
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing:          3
                        visible:          !!appController.selectedMatchData.matchId

                        Label {
                            text:           "Match Info"
                            color:          "#a89984"
                            font.pixelSize: 10
                            font.bold:      true
                        }
                        MetaField {
                            label: "Method"
                            value: appController.selectedMatchData.method || ""
                        }
                        MetaField {
                            label: "Confidence"
                            value: {
                                const c = appController.selectedMatchData.confidence
                                return (c && c > 0) ? Math.round(c) + "%" : ""
                            }
                        }
                    }

                    // ── File Paths ────────────────────────────────────────────
                    Rectangle {
                        Layout.fillWidth:   true
                        height:             1
                        color:              "#3c3836"
                        Layout.topMargin:   2
                        Layout.bottomMargin: 2
                        visible:            appController.selectedFileId > 0
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing:          3
                        visible:          appController.selectedFileId > 0

                        Label {
                            text:           "Paths"
                            color:          "#a89984"
                            font.pixelSize: 10
                            font.bold:      true
                        }

                        // ROM Path — the file's location on disk (original or moved-to-original_roms)
                        MetaField {
                            property bool   _origExists: appController.selectedFileData.originalExists || false
                            property bool   _currExists: appController.selectedFileData.currentExists  || false
                            property string _orig: appController.selectedFileData.originalPath || ""
                            property string _curr: appController.selectedFileData.path         || ""

                            // Show if original still exists, OR if file was moved (currentPath differs and exists)
                            visible: _origExists || (_currExists && _curr !== _orig &&
                                     !(appController.selectedFileData.isOrganized || false))
                            label:   "ROM Path"
                            value:   _origExists ? _orig : _curr
                        }

                        // Converted ROM — only when converted, not yet bundled or organized
                        MetaField {
                            property string _curr: appController.selectedFileData.path         || ""
                            property string _orig: appController.selectedFileData.originalPath || ""

                            visible: (appController.selectedFileData.isConverted  || false) &&
                                     !(appController.selectedFileData.isBundled   || false) &&
                                     !(appController.selectedFileData.isOrganized || false) &&
                                     _curr.length > 0 && _curr !== _orig
                            label:   "Converted ROM"
                            value:   _curr
                        }

                        // Bundle path — when bundled but not organized
                        MetaField {
                            visible: (appController.selectedFileData.isBundled    || false) &&
                                     !(appController.selectedFileData.isOrganized || false) &&
                                     (appController.selectedFileData.bundleOutputPath || "").length > 0
                            label:   "Bundle"
                            value:   appController.selectedFileData.bundleOutputPath || ""
                        }

                        // Organized destination
                        MetaField {
                            visible: (appController.selectedFileData.isOrganized  || false) &&
                                     (appController.selectedFileData.organizedPath || "").length > 0
                            label:   "Organized"
                            value:   appController.selectedFileData.organizedPath || ""
                        }
                    }

                    // Bottom padding
                    Item { Layout.preferredHeight: 12 }
                }
            }
        }
    }

    // ── Inline helper component: labelled metadata row ──────────────────────
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
            elide:            Text.ElideRight
            wrapMode:         mono ? Text.NoWrap : Text.WordWrap
        }
    }
}

