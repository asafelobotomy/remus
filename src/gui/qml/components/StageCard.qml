import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

// Collapsible stage section card.
// Declare child items directly inside StageCard { } — they appear in the
// content area and are hidden when the card is collapsed.
Frame {
    id: root

    required property string stageTitle
    property int    stageCount:  0
    property bool   expanded:    true

    // Emitted when the user clicks the collapse/expand toggle.
    // The parent is responsible for updating `expanded` in accordion mode.
    signal toggleRequested()

    // Children declared by users go into the content ColumnLayout
    default property alias items: contentArea.data

    Layout.fillWidth: true

    contentItem: ColumnLayout {
        spacing: 0

        // ── Header ──────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text:           root.stageTitle
                font.bold:      true
                font.pixelSize: Theme.fontXl
                color:          Theme.textPrimary
            }

            // Count badge
            Rectangle {
                visible:   root.stageCount > 0
                width:     badge.implicitWidth + 14
                height:    20
                radius:    10
                color:     Theme.success

                Label {
                    id:                   badge
                    anchors.centerIn:     parent
                    text:                 root.stageCount
                    color:                Theme.background
                    font.pixelSize:       Theme.fontSm
                    font.bold:            true
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text:     root.expanded ? "\u25B2" : "\u25BC"
                flat:     true
                padding:  4
                onClicked: root.toggleRequested()
            }
        }

        // Thin divider
        Rectangle {
            visible:        root.expanded
            Layout.fillWidth: true
            height:         1
            color:          Theme.border
            Layout.topMargin: 6
            Layout.bottomMargin: 6
        }

        // ── Content area ────────────────────────────────────────────────────
        ColumnLayout {
            id:               contentArea
            visible:          root.expanded
            Layout.fillWidth: true
            spacing:          10
        }
    }

    background: Rectangle {
        color:        Theme.surface
        border.color: Theme.border
        radius:       Theme.panelRadius
    }
}
