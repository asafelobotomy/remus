import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Centre ROM list — TMM-style table with status icon columns.
Frame {
    id: root

    Layout.fillWidth:  true
    Layout.fillHeight: true

    property string searchText: ""

    readonly property int colTitleMin: 180
    readonly property int colSystem:  120
    readonly property int colYear:    44
    readonly property int colMatch:   48
    readonly property int colStatus:  28
    readonly property int tableMinWidth:
        colTitleMin + colSystem + colYear + colMatch + (colStatus * 5)

    background: Rectangle {
        color:        "#1d2021"
        border.color: "#504945"
        radius:       12
    }

    ColumnLayout {
        anchors.fill:    parent
        anchors.margins: 8
        spacing:         6

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text:           "Library"
                font.bold:      true
                font.pixelSize: 14
                color:          "#fbf1c7"
            }

            TextField {
                id:               searchField
                Layout.fillWidth: true
                placeholderText:  "Search title, system, or path…"
                font.pixelSize:   12
                onTextChanged:    root.searchText = text
            }

            Label {
                text:           workflowController.queueFiles.length + " shown"
                color:          "#928374"
                font.pixelSize: 11
            }
        }

        ScrollView {
            Layout.fillWidth:  true
            Layout.preferredHeight: 28
            contentWidth:    headerRow.width
            ScrollBar.vertical.policy:   ScrollBar.AlwaysOff
            ScrollBar.horizontal.policy: ScrollBar.AsNeeded

            RowLayout {
                id: headerRow
                width: Math.max(root.tableMinWidth, root.width - 16)
                spacing: 0

                Label {
                    Layout.minimumWidth: root.colTitleMin
                    Layout.fillWidth:    true
                    text: "Title"
                    font.pixelSize: 10
                    font.bold:      true
                    color:          "#a89984"
                }
                Label {
                    Layout.preferredWidth: root.colSystem
                    text: "System"
                    font.pixelSize: 10
                    font.bold:      true
                    color:          "#a89984"
                    elide:          Text.ElideRight
                }
                Label {
                    Layout.preferredWidth: root.colYear
                    horizontalAlignment: Text.AlignHCenter
                    text: "Year"
                    font.pixelSize: 10
                    font.bold:      true
                    color:          "#a89984"
                }
                Label {
                    Layout.preferredWidth: root.colMatch
                    horizontalAlignment: Text.AlignRight
                    text: "Match"
                    font.pixelSize: 10
                    font.bold:      true
                    color:          "#a89984"
                }
                Label {
                    Layout.preferredWidth: root.colStatus
                    horizontalAlignment: Text.AlignHCenter
                    text: "ID"
                    font.pixelSize: 10
                    font.bold:      true
                    color:          "#a89984"
                }
                Label {
                    Layout.preferredWidth: root.colStatus
                    horizontalAlignment: Text.AlignHCenter
                    text: "Art"
                    font.pixelSize: 10
                    font.bold:      true
                    color:          "#a89984"
                }
                Label {
                    Layout.preferredWidth: root.colStatus
                    horizontalAlignment: Text.AlignHCenter
                    text: "Conv"
                    font.pixelSize: 10
                    font.bold:      true
                    color:          "#a89984"
                }
                Label {
                    Layout.preferredWidth: root.colStatus
                    horizontalAlignment: Text.AlignHCenter
                    text: "Ren"
                    font.pixelSize: 10
                    font.bold:      true
                    color:          "#a89984"
                }
                Label {
                    Layout.preferredWidth: root.colStatus
                    horizontalAlignment: Text.AlignHCenter
                    text: "Org"
                    font.pixelSize: 10
                    font.bold:      true
                    color:          "#a89984"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height:           1
            color:            "#504945"
        }

        ScrollView {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            clip:              true
            contentWidth:      romList.contentWidth

            ListView {
                id: romList
                width: Math.max(root.tableMinWidth, root.width - 16)
                height: contentHeight
                spacing: 2
                model: workflowController.queueFiles

                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: ItemDelegate {
                    id: rowDelegate

                    required property var modelData

                    property bool filterMatch: {
                        if (root.searchText.length === 0)
                            return true
                        const q = root.searchText.toLowerCase()
                        const fields = [
                            modelData.filename || "",
                            modelData.matchedTitle || "",
                            modelData.systemName || "",
                            modelData.path || "",
                            modelData.rawFilename || ""
                        ]
                        for (let i = 0; i < fields.length; ++i) {
                            if (fields[i].toLowerCase().indexOf(q) >= 0)
                                return true
                        }
                        return false
                    }

                    visible: filterMatch
                    height:  filterMatch ? implicitHeight : 0
                    width:   romList.width
                    padding: 4
                    highlighted: appController.selectedFileId === modelData.fileId

                    contentItem: RowLayout {
                        spacing: 0

                        Label {
                            Layout.minimumWidth: root.colTitleMin
                            Layout.fillWidth:    true
                            text:             rowDelegate.modelData.filename
                            elide:            Text.ElideRight
                            font.pixelSize:   12
                            color:            "#ebdbb2"
                        }

                        Label {
                            Layout.preferredWidth: root.colSystem
                            text:             rowDelegate.modelData.systemName || "—"
                            elide:            Text.ElideRight
                            font.pixelSize:   11
                            color:            "#a89984"
                        }

                        Label {
                            Layout.preferredWidth: root.colYear
                            horizontalAlignment: Text.AlignHCenter
                            text: {
                                const y = rowDelegate.modelData.releaseYear
                                return (y && y > 0) ? y.toString() : "—"
                            }
                            font.pixelSize: 11
                            color:          "#a89984"
                        }

                        Label {
                            Layout.preferredWidth: root.colMatch
                            horizontalAlignment: Text.AlignRight
                            text: {
                                const c = rowDelegate.modelData.confidence
                                if (!rowDelegate.modelData.hasAnyMatch || !c || c <= 0)
                                    return "—"
                                return Math.round(c) + "%"
                            }
                            font.pixelSize: 11
                            color: {
                                const c = rowDelegate.modelData.confidence
                                if (!rowDelegate.modelData.hasMatch)
                                    return rowDelegate.modelData.hasAnyMatch ? "#fabd2f" : "#665c54"
                                if (c >= 90) return "#b8bb26"
                                if (c >= 60) return "#fabd2f"
                                return "#fb4934"
                            }
                        }

                        StatusIcon {
                            Layout.preferredWidth: root.colStatus
                            state: rowDelegate.modelData.hasMatch     ? StatusIcon.Done
                                 : rowDelegate.modelData.hasAnyMatch ? StatusIcon.Warn
                                 : rowDelegate.modelData.hasHash     ? StatusIcon.Fail
                                 :                                     StatusIcon.Fail
                            tooltip: rowDelegate.modelData.hasMatch     ? "Match confirmed"
                                   : rowDelegate.modelData.hasAnyMatch ? "Match needs review"
                                   : rowDelegate.modelData.hasHash     ? "Hashed, not matched"
                                   :                                     "Needs hash and match"
                        }

                        StatusIcon {
                            Layout.preferredWidth: root.colStatus
                            state: rowDelegate.modelData.hasArtwork  ? StatusIcon.Done
                                 : rowDelegate.modelData.hasMatch    ? StatusIcon.Warn
                                 :                                     StatusIcon.Fail
                            tooltip: rowDelegate.modelData.hasArtwork  ? "Artwork downloaded"
                                   : rowDelegate.modelData.hasMatch    ? "Missing artwork"
                                   :                                     "Match required first"
                        }

                        StatusIcon {
                            Layout.preferredWidth: root.colStatus
                            state: !rowDelegate.modelData.isConvertible ? StatusIcon.Na
                                 : rowDelegate.modelData.isConverted   ? StatusIcon.Done
                                 :                                       StatusIcon.Fail
                            tooltip: !rowDelegate.modelData.isConvertible ? "Conversion not applicable"
                                   : rowDelegate.modelData.isConverted   ? "Converted"
                                   :                                       "Not converted"
                        }

                        StatusIcon {
                            Layout.preferredWidth: root.colStatus
                            state: rowDelegate.modelData.isBundled ? StatusIcon.Done : StatusIcon.Fail
                            tooltip: rowDelegate.modelData.isBundled ? "Bundled and renamed" : "Not bundled"
                        }

                        StatusIcon {
                            Layout.preferredWidth: root.colStatus
                            state: rowDelegate.modelData.isOrganized ? StatusIcon.Done : StatusIcon.Fail
                            tooltip: rowDelegate.modelData.isOrganized ? "Organized into library" : "Not organized"
                        }
                    }

                    onClicked: {
                        const fid = modelData.fileId
                        Qt.callLater(function() { appController.selectedFileId = fid })
                    }

                    background: Rectangle {
                        color:  parent.highlighted ? "#3f4d4f"
                              : parent.hovered     ? "#383838"
                              : "transparent"
                        radius: 4
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible:          appController.libraryOpen && workflowController.queueFiles.length === 0
            text:             "No ROMs in this filter. Try Update library or change the filter."
            color:            "#a89984"
            wrapMode:         Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Label {
            Layout.fillWidth: true
            visible:          !appController.libraryOpen
            text:             "Open a library to view ROMs."
            color:            "#a89984"
            wrapMode:         Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
