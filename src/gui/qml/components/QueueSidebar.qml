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

                    // Stage badge row
                    RowLayout {
                        spacing: 4

                        Rectangle {
                            visible:  !modelData.hasHash
                            width:    noHashLabel.implicitWidth + 8
                            height:   14
                            radius:   7
                            color:    "#cc241d"
                            Label {
                                id:               noHashLabel
                                anchors.centerIn: parent
                                text:             "no hash"
                                font.pixelSize:   9
                                color:            "#fbf1c7"
                            }
                        }
                        Rectangle {
                            visible:  modelData.hasHash && !modelData.hasArtwork
                            width:    noArtLabel.implicitWidth + 8
                            height:   14
                            radius:   7
                            color:    "#d79921"
                            Label {
                                id:               noArtLabel
                                anchors.centerIn: parent
                                text:             "no art"
                                font.pixelSize:   9
                                color:            "#1d2021"
                            }
                        }
                        Rectangle {
                            visible:  modelData.hasHash && modelData.hasArtwork
                            width:    doneLabel.implicitWidth + 8
                            height:   14
                            radius:   7
                            color:    "#689d6a"
                            Label {
                                id:               doneLabel
                                anchors.centerIn: parent
                                text:             "done"
                                font.pixelSize:   9
                                color:            "#1d2021"
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
