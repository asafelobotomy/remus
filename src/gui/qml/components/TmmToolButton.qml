import QtQuick
import QtQuick.Controls

// TMM-style toolbar button: themed icon above label, muted when disabled.
ToolButton {
    id: control

    required property string iconName

    display: ToolButton.TextUnderIcon
    padding: 6
    icon.name: iconName
    icon.width: 22
    icon.height: 22
    icon.color: enabled ? "#ebdbb2" : "#665c54"
    opacity: enabled ? 1.0 : 0.38
}
