import QtQuick 2.15
import QtQuick.Controls 2.15

// Themed Button that overrides KDE Breeze defaults with explicit styling
Button {
    id: control
    
    property bool isPrimary: false  // Set true for primary action buttons
    
    // Explicit background overriding system style
    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 40
        color: {
            if (!control.enabled) return theme.cardBg
            if (control.isPrimary) {
                return control.pressed ? Qt.darker(theme.primary, 1.2) : 
                       control.hovered ? Qt.lighter(theme.primary, 1.1) : theme.primary
            } else {
                return control.pressed ? Qt.darker(theme.cardBg, 1.1) : 
                       control.hovered ? Qt.lighter(theme.cardBg, 1.1) : theme.cardBg
            }
        }
        border.color: control.isPrimary ? theme.primary : theme.border
        border.width: 1
        radius: 4
    }
    
    // Explicit content item - KDE Breeze ignores palette.buttonText
    contentItem: Text {
        text: control.text
        font: control.font
        color: {
            if (!control.enabled) return theme.textMuted
            return control.isPrimary ? theme.primaryText : theme.textPrimary
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    
    // Padding for icon + text buttons
    leftPadding: icon.name ? 8 : 16
    rightPadding: 16
    topPadding: 8
    bottomPadding: 8
}
