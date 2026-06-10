import QtQuick
import QtQuick.Controls

// TMM-style pipeline status glyph for table columns.
Item {
    id: root

    enum State {
        Done = 0,
        Warn = 1,
        Fail = 2,
        Na   = 3
    }

    property int  state:   StatusIcon.Fail
    property string tooltip: ""

    implicitWidth:  26
    implicitHeight: 22

    ToolTip.visible: ma.containsMouse && tooltip.length > 0
    ToolTip.text:    tooltip
    ToolTip.delay:   400

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
    }

    readonly property var palette: ({
        [StatusIcon.Done]: { glyph: "\u2713", color: "#689d6a" },
        [StatusIcon.Warn]: { glyph: "\u25D0", color: "#d79921" },
        [StatusIcon.Fail]: { glyph: "\u2717", color: "#cc241d" },
        [StatusIcon.Na]:   { glyph: "\u2014", color: "#665c54" }
    })

    Label {
        anchors.centerIn: parent
        text:       palette[state].glyph
        color:      palette[state].color
        font.pixelSize: 13
        font.bold:  state !== StatusIcon.Na
    }
}
