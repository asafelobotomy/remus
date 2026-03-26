import QtQuick 2.15
import QtQuick.Controls 2.15

// Themed Switch that overrides KDE Breeze defaults with explicit styling
Switch {
    id: control
    
    // Explicit indicator (the toggle track and knob)
    indicator: Rectangle {
        implicitWidth: 48
        implicitHeight: 26
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 13
        color: control.checked ? theme.primary : theme.border
        border.color: control.checked ? theme.primary : theme.border
        
        // The sliding knob
        Rectangle {
            x: control.checked ? parent.width - width - 2 : 2
            y: 2
            width: 22
            height: 22
            radius: 11
            color: theme.textLight
            
            Behavior on x {
                NumberAnimation { duration: 100 }
            }
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
