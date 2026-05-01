import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Right-side inspector showing details for the selected file.
// Covers: artwork preview, file info, match actions, metadata, package, place.
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
            spacing: 12

            // Placeholder when nothing selected
            Label {
                visible:         appController.selectedFileId <= 0
                Layout.fillWidth: true
                text:            workflowController.hint
                wrapMode:        Text.WordWrap
                color:           "#a89984"
                font.pixelSize:  13
                horizontalAlignment: Text.AlignHCenter
                topPadding:      32
            }

            // ── Content (file selected) ─────────────────────────────────────
            ColumnLayout {
                visible:          appController.selectedFileId > 0
                Layout.fillWidth: true
                spacing:          10

                // Artwork
                Image {
                    Layout.fillWidth:  true
                    Layout.preferredHeight: 160
                    source:            artworkController.previewUrl
                    fillMode:          Image.PreserveAspectFit
                    clip:              true

                    Rectangle {
                        anchors.fill: parent
                        color:        "#282828"
                        visible:      parent.status !== Image.Ready
                        Label {
                            anchors.centerIn: parent
                            text:             "No artwork"
                            color:            "#665c54"
                        }
                    }
                }

                // File info
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing:          4

                    Label {
                        Layout.fillWidth: true
                        text:    appController.selectedFile().filename || ""
                        elide:   Text.ElideRight
                        font.bold:       true
                        font.pixelSize:  13
                        color:           "#fbf1c7"
                    }
                    Label {
                        text:  appController.selectedFile().systemName || ""
                        color: "#a89984"
                        font.pixelSize: 11
                    }
                }

                // Hint label
                Label {
                    Layout.fillWidth: true
                    text:    workflowController.hint
                    wrapMode: Text.WordWrap
                    color:   "#83a598"
                    font.pixelSize: 11
                }

                // ── Match section ──────────────────────────────────────────
                Frame {
                    Layout.fillWidth: true
                    background: Rectangle {
                        color: "#282828"; border.color: "#504945"; radius: 8
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing:      6

                        Label {
                            text:            appController.selectedMatch().title || "No match"
                            font.bold:       true
                            font.pixelSize:  12
                            color:           "#fbf1c7"
                            elide:           Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Label {
                            visible: appController.selectedMatch().confidence > 0
                            text:    "Confidence: " +
                                     Math.round(appController.selectedMatch().confidence * 100) + "%"
                            color:   "#a89984"
                            font.pixelSize: 11
                        }

                        RowLayout {
                            spacing: 6

                            Button {
                                text:     "✓ Confirm"
                                enabled:  appController.selectedFileId > 0 &&
                                          !appController.selectedMatch().confirmed
                                font.pixelSize: 11
                                padding:  6
                                onClicked: matchController.confirmSelected()
                            }
                            Button {
                                text:     "✗ Reject"
                                enabled:  appController.selectedFileId > 0 &&
                                          !appController.selectedMatch().rejected
                                font.pixelSize: 11
                                padding:  6
                                onClicked: matchController.rejectSelected()
                            }
                        }
                    }
                }

                // ── Artwork section ────────────────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    spacing:          6

                    Button {
                        text:       "Get Artwork"
                        enabled:    appController.selectedFileId > 0
                        font.pixelSize: 11
                        padding:    6
                        onClicked:  artworkController.downloadSelected()
                    }
                    ProgressBar {
                        Layout.fillWidth:  true
                        from:              0
                        to:                artworkController.downloadTotal || 1
                        value:             artworkController.downloadProgress
                        visible:           artworkController.downloading
                    }
                }

                // ── Package section ────────────────────────────────────────
                Frame {
                    Layout.fillWidth: true
                    background: Rectangle {
                        color: "#282828"; border.color: "#504945"; radius: 8
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        spacing:      6

                        Label {
                            text: "Package"
                            font.bold: true
                            font.pixelSize: 12
                            color: "#fbf1c7"
                        }

                        ComboBox {
                            id:               formatCombo
                            Layout.fillWidth: true
                            model:            ["CHD", "CSO", "Original"]
                            currentIndex:     0
                            font.pixelSize:   11
                        }

                        RowLayout {
                            spacing: 6
                            Button {
                                text:       "Convert"
                                enabled:    appController.selectedFileId > 0 &&
                                            !conversionController.converting
                                font.pixelSize: 11
                                padding:    6
                                onClicked:  conversionController.convertSelected(
                                                formatCombo.currentText, "")
                            }
                            Button {
                                text:       "Bundle"
                                enabled:    appController.selectedFileId > 0 &&
                                            !exportController.exporting
                                font.pixelSize: 11
                                padding:    6
                                onClicked:  exportController.bundleSelected("")
                            }
                        }

                        Label {
                            visible:   conversionController.lastMessage.length > 0
                            text:      conversionController.lastMessage
                            color:     "#a89984"
                            wrapMode:  Text.WordWrap
                            font.pixelSize: 10
                            Layout.fillWidth: true
                        }
                    }
                }

                // ── Place section ──────────────────────────────────────────
                RowLayout {
                    Layout.fillWidth: true
                    spacing:          6

                    Button {
                        text:       "Preview Org."
                        enabled:    appController.selectedFileId > 0
                        font.pixelSize: 11
                        padding:    6
                        onClicked:  organizeController.previewOrganize("")
                    }
                    Button {
                        text:       "Apply Org."
                        enabled:    appController.selectedFileId > 0 &&
                                    !organizeController.organizing
                        font.pixelSize: 11
                        padding:    6
                        onClicked:  organizeController.applyOrganize("")
                    }
                }
            }
        }
    }
}
