import QtQuick 2.15
import QtQuick.Controls 2.15

// Themed Slider that overrides KDE Breeze defaults with explicit styling
Slider {
    id: control
    
    // Explicit background (the track)
    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 200
        implicitHeight: 6
        width: control.availableWidth
        height: implicitHeight
        radius: 3
        color: theme.cardBg
        border.color: theme.border
        border.width: 1
        
        // Filled portion
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            color: theme.primary
            radius: 3
        }
    }
    
    // Explicit handle (the draggable knob)
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 20
        implicitHeight: 20
        radius: 10
        color: control.pressed ? Qt.darker(theme.primary, 1.1) : theme.primary
        border.color: theme.border
        border.width: 1
    }
}
