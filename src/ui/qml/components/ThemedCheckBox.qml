import QtQuick 2.15
import QtQuick.Controls 2.15

// Themed CheckBox that overrides KDE Breeze defaults with explicit styling
CheckBox {
    id: control
    
    // Explicit indicator (the checkbox square)
    indicator: Rectangle {
        implicitWidth: 22
        implicitHeight: 22
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 3
        color: control.enabled ? theme.mainBg : theme.cardBg
        border.color: control.checked ? theme.primary : theme.border
        border.width: control.checked ? 2 : 1
        
        // Checkmark
        Rectangle {
            width: 12
            height: 12
            x: 5
            y: 5
            radius: 2
            color: theme.primary
            visible: control.checked
        }
    }
    
    // Explicit content item - label text
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.enabled ? theme.textPrimary : theme.textMuted
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
    
    spacing: 8
}
