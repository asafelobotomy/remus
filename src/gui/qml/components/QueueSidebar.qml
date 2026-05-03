import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Left-side queue showing files bucketed by pipeline stage.
// Reads stage counts from workflowController; file list from queueFiles.
Frame {
    id: root

    Layout.fillHeight: true

    background: Rectangle {
        color:        "#1d2021"
        border.color: "#504945"
        radius:       12
    }

    // Stage filter constants (mirror WorkflowController::Stage enum)
    readonly property int stageAll:      0
    readonly property int stageIdentity: 1
    readonly property int stageEnrich:   2
    readonly property int stageDone:     3

    ColumnLayout {
        anchors.fill:    parent
        anchors.margins: 8
        spacing:         6

        Label {
            text:           "Queue"
            font.bold:      true
            font.pixelSize: 13
            color:          "#a89984"
        }

        // ── Stage filter buttons ─────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            spacing:          2

            Repeater {
                model: [
                    { label: "All",            stage: root.stageAll,      count: -1                                          },
                    { label: "Needs ID",       stage: root.stageIdentity, count: workflowController.identityCount },
                    { label: "Needs Artwork",  stage: root.stageEnrich,   count: workflowController.enrichCount   },
                    { label: "Done",           stage: root.stageDone,     count: workflowController.doneCount     }
                ]

                Button {
                    required property var modelData

                    Layout.fillWidth: true
                    checkable:        true
                    checked:          workflowController.queueStage === modelData.stage
                    text:             modelData.count >= 0
                                        ? modelData.label + " (" + modelData.count + ")"
                                        : modelData.label
                    font.pixelSize:   11
                    padding:          4
                    onClicked:        workflowController.queueStage = modelData.stage
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height:           1
            color:            "#504945"
        }

        // ── File list ────────────────────────────────────────────────────────
        ListView {
            id:               fileList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip:             true
            spacing:          2
            model:            workflowController.queueFiles

            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id:                     fileItem
                required property var   modelData

                // Pre-compute extension list (primary + children) for chip display
                property var fileExtensions: {
                    const exts = []
                    const prim = modelData.extension || ""
                    if (prim.length > 0) exts.push(prim)
                    const children = modelData.childExtensions || ""
                    if (children.length > 0)
                        children.split(",").forEach(e => { if (e.trim().length > 0) exts.push(e.trim()) })
                    return exts
                }

                width:          ListView.view.width
                padding:        6
                highlighted:    appController.selectedFileId === modelData.fileId

                contentItem: ColumnLayout {
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text:             modelData.filename
                        elide:            Text.ElideRight
                        font.pixelSize:   11
                        color:            "#ebdbb2"
                    }

                    // Extension chips row (e.g. .bin + .cue for a grouped disc image)
                    Flow {
                        Layout.fillWidth: true
                        spacing:          3
                        visible:          fileItem.fileExtensions.length > 0

                        Repeater {
                            model: fileItem.fileExtensions
                            Rectangle {
                                width:  extLabel.implicitWidth + 8
                                height: 13
                                radius: 3
                                color:  "#3c3836"
                                Label {
                                    id:               extLabel
                                    anchors.centerIn: parent
                                    text:             modelData
                                    font.pixelSize:   9
                                    color:            "#bdae93"
                                }
                            }
                        }
                    }

                    // Pipeline completion badges — single rect per stage, colour-coded:
                    //   RED    = process not yet run
                    //   YELLOW = H&M only: match found but not confirmed
                    //   GREEN  = completed successfully
                    Flow {
                        width:   parent.width
                        spacing: 3

                        // ── H&M ──────────────────────────────────────────────
                        Rectangle {
                            width:  hmBadge.implicitWidth + 8
                            height: 14
                            radius: 7
                            color:  modelData.hasMatch     ? "#689d6a"
                                  : modelData.hasAnyMatch  ? "#d79921"
                                  :                          "#cc241d"
                            Label {
                                id:               hmBadge
                                anchors.centerIn: parent
                                text:             "H&M"
                                font.pixelSize:   9
                                color:            modelData.hasMatch ? "#1d2021"
                                                : modelData.hasAnyMatch ? "#1d2021"
                                                : "#fbf1c7"
                            }
                        }

                        // ── Art ───────────────────────────────────────────────
                        Rectangle {
                            width:  artBadge.implicitWidth + 8
                            height: 14
                            radius: 7
                            color:  modelData.hasArtwork ? "#689d6a" : "#cc241d"
                            Label {
                                id:               artBadge
                                anchors.centerIn: parent
                                text:             "Art"
                                font.pixelSize:   9
                                color:            modelData.hasArtwork ? "#1d2021" : "#fbf1c7"
                            }
                        }

                        // ── Bndl ─────────────────────────────────────────────
                        Rectangle {
                            width:  bndlBadge.implicitWidth + 8
                            height: 14
                            radius: 7
                            color:  modelData.isBundled ? "#689d6a" : "#cc241d"
                            Label {
                                id:               bndlBadge
                                anchors.centerIn: parent
                                text:             "Bndl"
                                font.pixelSize:   9
                                color:            modelData.isBundled ? "#1d2021" : "#fbf1c7"
                            }
                        }

                        // ── Org ───────────────────────────────────────────────
                        Rectangle {
                            width:  orgBadge.implicitWidth + 8
                            height: 14
                            radius: 7
                            color:  modelData.isOrganized ? "#689d6a" : "#cc241d"
                            Label {
                                id:               orgBadge
                                anchors.centerIn: parent
                                text:             "Org"
                                font.pixelSize:   9
                                color:            modelData.isOrganized ? "#1d2021" : "#fbf1c7"
                            }
                        }

                        // ── Conv (disc/image formats only) ────────────────────
                        Rectangle {
                            visible: modelData.isConvertible
                            width:   convBadge.implicitWidth + 8
                            height:  14
                            radius:  7
                            color:   modelData.isConverted ? "#689d6a" : "#cc241d"
                            Label {
                                id:               convBadge
                                anchors.centerIn: parent
                                text:             "Conv"
                                font.pixelSize:   9
                                color:            modelData.isConverted ? "#1d2021" : "#fbf1c7"
                            }
                        }
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
                    radius: 6
                }
            }
        }
    }
}
