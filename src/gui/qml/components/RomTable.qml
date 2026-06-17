import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

// Centre ROM list — TMM-style table with status icon columns.
Frame {
    id: root

    Layout.fillWidth:  true
    Layout.fillHeight: true

    property string searchText: ""

    readonly property string romSourceDirectory: {
        const fromSettings = settingsController.stringValue("gui/rom_source_directory", "")
        if (fromSettings.length > 0)
            return fromSettings
        return scanController.lastDirectory
    }

    readonly property int colTitleMin: 180
    readonly property int colSystem:  120
    readonly property int colYear:    44
    readonly property int colMatch:   48
    readonly property int colStatus:  28
    readonly property int tableMinWidth:
        colTitleMin + colSystem + colYear + colMatch + (colStatus * 5)

    background: Rectangle {
        color:        Theme.panelBg
        border.color: Theme.panelBorder
        radius:       Theme.panelRadius
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
                font.pixelSize: Theme.fontXl
                color:          Theme.textPrimary
            }

            TextField {
                id:               searchField
                Layout.fillWidth: true
                placeholderText:  "Search title, system, or path\u2026"
                font.pixelSize:   Theme.fontMd
                onTextChanged:    root.searchText = text
            }

            Label {
                text:           workflowController.queueFiles.length + " shown"
                color:          Theme.textDim
                font.pixelSize: Theme.fontSm
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
                    font.pixelSize: Theme.fontXs
                    font.bold:      true
                    color:          Theme.textMuted
                }
                Label {
                    Layout.preferredWidth: root.colSystem
                    text: "System"
                    font.pixelSize: Theme.fontXs
                    font.bold:      true
                    color:          Theme.textMuted
                    elide:          Text.ElideRight
                }
                Label {
                    Layout.preferredWidth: root.colYear
                    horizontalAlignment: Text.AlignHCenter
                    text: "Year"
                    font.pixelSize: Theme.fontXs
                    font.bold:      true
                    color:          Theme.textMuted
                }
                Label {
                    Layout.preferredWidth: root.colMatch
                    horizontalAlignment: Text.AlignRight
                    text: "Match"
                    font.pixelSize: Theme.fontXs
                    font.bold:      true
                    color:          Theme.textMuted
                }
                Label {
                    Layout.preferredWidth: root.colStatus
                    horizontalAlignment: Text.AlignHCenter
                    text: "ID"
                    font.pixelSize: Theme.fontXs
                    font.bold:      true
                    color:          Theme.textMuted
                }
                Label {
                    Layout.preferredWidth: root.colStatus
                    horizontalAlignment: Text.AlignHCenter
                    text: "Art"
                    font.pixelSize: Theme.fontXs
                    font.bold:      true
                    color:          Theme.textMuted
                }
                Label {
                    Layout.preferredWidth: root.colStatus
                    horizontalAlignment: Text.AlignHCenter
                    text: "Conv"
                    font.pixelSize: Theme.fontXs
                    font.bold:      true
                    color:          Theme.textMuted
                }
                Label {
                    Layout.preferredWidth: root.colStatus
                    horizontalAlignment: Text.AlignHCenter
                    text: "Ren"
                    font.pixelSize: Theme.fontXs
                    font.bold:      true
                    color:          Theme.textMuted
                }
                Label {
                    Layout.preferredWidth: root.colStatus
                    horizontalAlignment: Text.AlignHCenter
                    text: "Org"
                    font.pixelSize: Theme.fontXs
                    font.bold:      true
                    color:          Theme.textMuted
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height:           1
            color:            Theme.border
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

                    readonly property string rowType: modelData.rowType || "file"
                    readonly property bool isGroup: rowType === "group"
                    readonly property bool isDisc: rowType === "disc"

                    property bool filterMatch: {
                        if (root.searchText.length === 0)
                            return true
                        const q = root.searchText.toLowerCase()
                        if (isGroup) {
                            const blob = (modelData.filename || "") + " " + (modelData.memberSearchText || "")
                            return blob.toLowerCase().indexOf(q) >= 0
                        }
                        const fields = [
                            modelData.filename || "",
                            modelData.discLabel || "",
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
                    highlighted: !isGroup && appController.selectedFileId === modelData.fileId

                    contentItem: RowLayout {
                        spacing: 0

                        Item {
                            Layout.preferredWidth: isDisc ? 18 : (isGroup ? 22 : 0)
                            Layout.alignment: Qt.AlignVCenter

                            Label {
                                visible: isGroup
                                anchors.centerIn: parent
                                text: modelData.expanded ? "\u25BE" : "\u25B8"
                                font.pixelSize: Theme.fontSm
                                color: Theme.textMuted
                            }
                        }

                        Label {
                            Layout.minimumWidth: root.colTitleMin - (isDisc ? 18 : (isGroup ? 22 : 0))
                            Layout.fillWidth:    true
                            text: {
                                if (isGroup)
                                    return modelData.filename + "  (" + modelData.discCount + " discs)"
                                if (isDisc)
                                    return modelData.discLabel || modelData.filename
                                return rowDelegate.modelData.filename
                            }
                            elide:            Text.ElideRight
                            font.pixelSize:   Theme.fontMd
                            font.bold:        isGroup
                            color:            isGroup ? Theme.textPrimary : Theme.textBody
                            leftPadding:      isDisc ? 4 : 0
                        }

                        Label {
                            Layout.preferredWidth: root.colSystem
                            text:             rowDelegate.modelData.systemName || "\u2014"
                            elide:            Text.ElideRight
                            font.pixelSize:   Theme.fontSm
                            color:            Theme.textMuted
                            visible:          !isGroup
                        }

                        Label {
                            Layout.preferredWidth: root.colYear
                            horizontalAlignment: Text.AlignHCenter
                            visible:          !isGroup
                            text: {
                                const y = rowDelegate.modelData.releaseYear
                                return (y && y > 0) ? y.toString() : "\u2014"
                            }
                            font.pixelSize: Theme.fontSm
                            color:          Theme.textMuted
                        }

                        Label {
                            Layout.preferredWidth: root.colMatch
                            horizontalAlignment: Text.AlignRight
                            text: {
                                if (isGroup)
                                    return modelData.matchProgress || "\u2014"
                                const c = rowDelegate.modelData.confidence
                                if (!rowDelegate.modelData.hasAnyMatch || !c || c <= 0)
                                    return "\u2014"
                                return Math.round(c) + "%"
                            }
                            font.pixelSize: Theme.fontSm
                            color: {
                                if (isGroup) {
                                    if (modelData.hasMatch)
                                        return Theme.success
                                    if (modelData.hasAnyMatch)
                                        return Theme.warn
                                    return Theme.textDisabled
                                }
                                const c = rowDelegate.modelData.confidence
                                if (!rowDelegate.modelData.hasMatch)
                                    return rowDelegate.modelData.hasAnyMatch ? Theme.warn : Theme.textDisabled
                                if (c >= 90) return Theme.success
                                if (c >= 60) return Theme.warn
                                return Theme.error
                            }
                        }

                        StatusIcon {
                            Layout.preferredWidth: root.colStatus
                            state: isGroup
                                 ? (modelData.hasMatch     ? StatusIcon.Done
                                   : modelData.hasAnyMatch ? StatusIcon.Warn
                                   : modelData.hasHash     ? StatusIcon.Fail
                                   :                         StatusIcon.Fail)
                                 : (rowDelegate.modelData.hasMatch     ? StatusIcon.Done
                                   : rowDelegate.modelData.hasAnyMatch ? StatusIcon.Warn
                                   : rowDelegate.modelData.hasHash     ? StatusIcon.Fail
                                   :                                     StatusIcon.Fail)
                            tooltip: isGroup
                                   ? (modelData.hasMatch     ? "All discs matched"
                                     : modelData.hasAnyMatch ? "Some discs need match review"
                                     : modelData.hasHash     ? "Hashed, not matched"
                                     :                         "Needs hash and match")
                                   : (rowDelegate.modelData.hasMatch     ? "Match confirmed"
                                     : rowDelegate.modelData.hasAnyMatch ? "Match needs review"
                                     : rowDelegate.modelData.hasHash     ? "Hashed, not matched"
                                     :                                     "Needs hash and match")
                        }

                        StatusIcon {
                            Layout.preferredWidth: root.colStatus
                            state: isGroup
                                 ? (modelData.hasArtwork ? StatusIcon.Done
                                   : modelData.hasMatch  ? StatusIcon.Warn
                                   :                       StatusIcon.Fail)
                                 : (rowDelegate.modelData.hasArtwork  ? StatusIcon.Done
                                   : rowDelegate.modelData.hasMatch    ? StatusIcon.Warn
                                   :                                     StatusIcon.Fail)
                            tooltip: isGroup
                                   ? (modelData.hasArtwork ? "Artwork complete (" + (modelData.artworkProgress || "") + ")"
                                     : modelData.hasMatch  ? "Missing artwork"
                                     :                       "Match required first")
                                   : (rowDelegate.modelData.hasArtwork  ? "Artwork downloaded"
                                     : rowDelegate.modelData.hasMatch    ? "Missing artwork"
                                     :                                     "Match required first")
                        }

                        StatusIcon {
                            Layout.preferredWidth: root.colStatus
                            state: isGroup
                                 ? (!modelData.isConvertible ? StatusIcon.Na
                                   : modelData.isConverted   ? StatusIcon.Done
                                   :                           StatusIcon.Fail)
                                 : (!rowDelegate.modelData.isConvertible ? StatusIcon.Na
                                   : rowDelegate.modelData.isConverted   ? StatusIcon.Done
                                   :                                       StatusIcon.Fail)
                            tooltip: isGroup
                                   ? (!modelData.isConvertible ? "Conversion not applicable"
                                     : modelData.isConverted   ? "All discs converted"
                                     :                           "Not all discs converted")
                                   : (!rowDelegate.modelData.isConvertible ? "Conversion not applicable"
                                     : rowDelegate.modelData.isConverted   ? "Converted"
                                     :                                       "Not converted")
                        }

                        StatusIcon {
                            Layout.preferredWidth: root.colStatus
                            state: isGroup
                                 ? (modelData.isBundled ? StatusIcon.Done : StatusIcon.Fail)
                                 : (rowDelegate.modelData.isBundled ? StatusIcon.Done : StatusIcon.Fail)
                            tooltip: isGroup
                                   ? (modelData.isBundled ? "All discs bundled" : "Not all discs bundled")
                                   : (rowDelegate.modelData.isBundled ? "Bundled and renamed" : "Not bundled")
                        }

                        StatusIcon {
                            Layout.preferredWidth: root.colStatus
                            state: isGroup
                                 ? (modelData.isOrganized ? StatusIcon.Done : StatusIcon.Fail)
                                 : (rowDelegate.modelData.isOrganized ? StatusIcon.Done : StatusIcon.Fail)
                            tooltip: isGroup
                                   ? (modelData.isOrganized ? "All discs organized" : "Not all discs organized")
                                   : (rowDelegate.modelData.isOrganized ? "Organized into library" : "Not organized")
                        }
                    }

                    onClicked: {
                        if (isGroup) {
                            workflowController.toggleDiscGroupExpanded(modelData.groupKey)
                            return
                        }
                        const fid = modelData.fileId
                        Qt.callLater(function() { appController.selectedFileId = fid })
                    }

                    background: Rectangle {
                        color:  parent.highlighted ? Theme.selected
                              : parent.hovered     ? Theme.hover
                              : isGroup              ? Theme.surfaceHigh
                              : "transparent"
                        radius: 4
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible:          appController.libraryOpen && workflowController.queueFiles.length === 0
            text:             romSourceDirectory.length === 0
                                ? "No ROMs yet. Set a ROM source folder in Settings, then click Update library."
                                : "No ROMs in this filter. Try Update library or change the filter."
            color:            Theme.textMuted
            wrapMode:         Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        Label {
            Layout.fillWidth: true
            visible:          !appController.libraryOpen
            text:             "Open a library database above, then scan your ROM folder with Update library."
            color:            Theme.textMuted
            wrapMode:         Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
