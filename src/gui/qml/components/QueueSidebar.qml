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
                    { label: "All",            stage: root.stageAll,      count: -1, icon: "view-list-symbolic" },
                    { label: "Needs ID",       stage: root.stageIdentity, count: workflowController.identityCount, icon: "dialog-question-symbolic" },
                    { label: "Needs Artwork",  stage: root.stageEnrich,   count: workflowController.enrichCount, icon: "image-x-generic-symbolic" },
                    { label: "Done",           stage: root.stageDone,     count: workflowController.doneCount, icon: "emblem-ok-symbolic" }
                ]

                Button {
                    required property var modelData

                    Layout.fillWidth: true
                    checkable:        true
                    checked:          workflowController.queueStage === modelData.stage
                    text:             modelData.count >= 0
                                        ? modelData.label + " (" + modelData.count + ")"
                                        : modelData.label
                    display:          AbstractButton.TextBesideIcon
                    icon.name:        modelData.icon
                    icon.width:       14
                    icon.height:      14
                    icon.color:       checked ? "#fbf1c7" : "#928374"
                    font.pixelSize:   11
                    padding:          4
                    opacity:          appController.libraryOpen ? 1.0 : 0.38
                    enabled:          appController.libraryOpen
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
            display:          AbstractButton.TextBesideIcon
            icon.name:        "view-refresh-symbolic"
            icon.width:       14
            icon.height:      14
            text:             "Refresh"
            font.pixelSize:   11
            opacity:          appController.libraryOpen ? 1.0 : 0.38
            enabled:          appController.libraryOpen
            onClicked:        workflowController.refresh()
        }
    }
}
