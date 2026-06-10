import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Stage filter chips for the library queue (file list lives in RomTable).
Frame {
    id: root

    Layout.fillHeight: true

    background: Rectangle {
        color:        "#1d2021"
        border.color: "#504945"
        radius:       12
    }

    readonly property int stageAll:      0
    readonly property int stageIdentity: 1
    readonly property int stageEnrich:   2
    readonly property int stageDone:     3

    ColumnLayout {
        anchors.fill:    parent
        anchors.margins: 8
        spacing:         6

        Label {
            text:           "Filter"
            font.bold:      true
            font.pixelSize: 13
            color:          "#a89984"
        }

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

        Label {
            Layout.fillWidth: true
            wrapMode:         Text.WordWrap
            color:            "#928374"
            font.pixelSize:   10
            text:             "Needs ID: unhashed or unmatched.\n"
                            + "Needs Artwork: matched, no box art.\n"
                            + "Done: matched with artwork."
        }

        Item { Layout.fillHeight: true }

        Button {
            Layout.fillWidth: true
            flat:             true
            text:             "↻ Refresh"
            font.pixelSize:   11
            onClicked:        workflowController.refresh()
        }
    }
}
