import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Remus.Gui

// Key–value label pair used throughout inspector panels and dialogs.
// label:     left-hand caption (bold, muted)
// value:     right-hand value text
// bold:      bold value text
// mono:      monospace value font (hash fields)
// minWidth:  minimum width of the label column (defaults to 72)
RowLayout {
    property string label:    ""
    property string value:    ""
    property bool   bold:     false
    property bool   mono:     false
    property int    minWidth: 72

    Layout.fillWidth: true
    spacing: 6

    Label {
        text:           label + ":"
        color:          Theme.textMuted
        font.pixelSize: Theme.fontXs
        font.bold:      true
        Layout.minimumWidth: minWidth
    }
    Label {
        Layout.fillWidth: true
        text:             value.length > 0 ? value : "\u2014"
        color:            value.length > 0 ? Theme.textBody : Theme.borderSub
        font.pixelSize:   Theme.fontSm
        font.bold:        bold
        font.family:      mono ? "monospace" : font.family
        elide:            mono ? Text.ElideMiddle : Text.ElideRight
        wrapMode:         mono ? Text.NoWrap : Text.WordWrap
    }
}
