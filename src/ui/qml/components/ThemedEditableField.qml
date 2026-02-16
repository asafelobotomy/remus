import QtQuick 2.15
import QtQuick.Controls 2.15

// Themed editable TextField that adjusts styling based on readOnly property
// Used for detail views where fields can switch between view and edit modes
TextField {
    id: control
    
    // Explicit background that adapts to edit mode
    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 40
        color: control.readOnly ? "transparent" : theme.contentBg
        border.color: control.activeFocus ? theme.primary : (control.readOnly ? "transparent" : theme.border)
        border.width: control.readOnly ? 0 : 1
        radius: 4
    }
    
    // Explicit text color - KDE Breeze ignores palette.text
    color: theme.textPrimary
    placeholderTextColor: theme.textMuted
    selectionColor: theme.primary
    selectedTextColor: theme.textLight
    
    // Padding for proper text positioning
    leftPadding: control.readOnly ? 0 : 12
    rightPadding: control.readOnly ? 0 : 12
    topPadding: 8
    bottomPadding: 8
}
