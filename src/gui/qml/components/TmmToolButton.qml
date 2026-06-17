import QtQuick
import QtQuick.Controls
import Remus.Gui

// TMM-style toolbar button: themed icon above label, muted when disabled.
ToolButton {
    id: control

    required property string iconName

    display: ToolButton.TextUnderIcon
    padding: 6
    icon.name: iconName
    icon.width: 22
    icon.height: 22
    icon.color: enabled ? Theme.textBody : Theme.textDisabled
    opacity: enabled ? 1.0 : 0.38
}
