import QtQuick 2.15
import QtQuick.Controls 2.15

// Themed TextArea that overrides KDE Breeze defaults with explicit styling
TextArea {
    id: control
    
    // Explicit background overriding system style
    background: Rectangle {
        implicitWidth: 300
        implicitHeight: 100
        color: control.enabled ? theme.mainBg : theme.cardBg
        border.color: control.activeFocus ? theme.primary : theme.border
        border.width: control.activeFocus ? 2 : 1
        radius: 4
    }
    
    // Explicit text color - KDE Breeze ignores palette.text
    color: theme.textPrimary
    placeholderTextColor: theme.textMuted
    selectionColor: theme.primary
    selectedTextColor: theme.textLight
    
    // Padding for proper text positioning
    leftPadding: 12
    rightPadding: 12
    topPadding: 12
    bottomPadding: 12
    
    wrapMode: TextEdit.Wrap
}
