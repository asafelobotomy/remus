import QtQuick 2.15
import QtQuick.Controls 2.15

// Themed ProgressBar that overrides KDE Breeze defaults with explicit styling
ProgressBar {
    id: control
    
    // Explicit background (the track)
    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 8
        color: theme.cardBg
        radius: 4
        border.color: theme.border
        border.width: 1
    }
    
    // Explicit content item (the filled portion)
    contentItem: Item {
        implicitWidth: 200
        implicitHeight: 8
        
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 4
            color: theme.primary
            
            // Indeterminate animation
            Behavior on width {
                enabled: !control.indeterminate
                NumberAnimation { duration: 150 }
            }
        }
        
        // Indeterminate mode: sliding bar
        Rectangle {
            visible: control.indeterminate
            x: (parent.width - width) * indeterminateAnimation.value
            width: parent.width * 0.3
            height: parent.height
            radius: 4
            color: theme.primary
            
            NumberAnimation on x {
                id: indeterminateAnimation
                property real value: 0
                running: control.indeterminate && control.visible
                from: 0
                to: 1
                duration: 1500
                loops: Animation.Infinite
            }
        }
    }
}
