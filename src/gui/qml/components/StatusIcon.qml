import QtQuick
import QtQuick.Controls
import Remus.Gui

// TMM-style pipeline status glyph for table columns.
Item {
    id: root

    enum State {
        Done = 0,
        Warn = 1,
        Fail = 2,
        Na   = 3
    }

    property int    state:   StatusIcon.Fail
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

    readonly property var glyphMap: ({
        [StatusIcon.Done]: { glyph: "\u2713", color: Theme.successIcon },
        [StatusIcon.Warn]: { glyph: "\u25D0", color: Theme.warnIcon },
        [StatusIcon.Fail]: { glyph: "\u2717", color: Theme.errorIcon },
        [StatusIcon.Na]:   { glyph: "\u2014", color: Theme.textDisabled }
    })

    Label {
        anchors.centerIn: parent
        text:       glyphMap[state].glyph
        color:      glyphMap[state].color
        font.pixelSize: Theme.fontLg
        font.bold:  state !== StatusIcon.Na
    }
}
