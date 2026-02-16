import QtQuick 2.15
import QtQuick.Controls 2.15

// Themed SpinBox that overrides KDE Breeze defaults with explicit styling
SpinBox {
    id: control
    
    // Explicit background
    background: Rectangle {
        implicitWidth: 140
        implicitHeight: 40
        color: control.enabled ? theme.mainBg : theme.cardBg
        border.color: control.activeFocus ? theme.primary : theme.border
        border.width: control.activeFocus ? 2 : 1
        radius: 4
    }
    
    // Explicit content item for the value display
    contentItem: TextInput {
        z: 2
        text: control.textFromValue(control.value, control.locale)
        font: control.font
        color: control.enabled ? theme.textPrimary : theme.textMuted
        selectionColor: theme.primary
        selectedTextColor: theme.textLight
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }
    
    // Up button (right side)
    up.indicator: Rectangle {
        x: control.mirrored ? 0 : parent.width - width
        height: parent.height
        implicitWidth: 40
        implicitHeight: 40
        color: control.up.pressed ? Qt.darker(theme.cardBg, 1.1) : 
               control.up.hovered ? Qt.lighter(theme.cardBg, 1.1) : theme.cardBg
        border.color: theme.border
        border.width: 1
        radius: 4
        
        Text {
            text: "+"
            font.pixelSize: 16
            font.bold: true
            color: control.enabled ? theme.textPrimary : theme.textMuted
            anchors.centerIn: parent
        }
    }
    
    // Down button (left side)
    down.indicator: Rectangle {
        x: control.mirrored ? parent.width - width : 0
        height: parent.height
        implicitWidth: 40
        implicitHeight: 40
        color: control.down.pressed ? Qt.darker(theme.cardBg, 1.1) : 
               control.down.hovered ? Qt.lighter(theme.cardBg, 1.1) : theme.cardBg
        border.color: theme.border
        border.width: 1
        radius: 4
        
        Text {
            text: "-"
            font.pixelSize: 16
            font.bold: true
            color: control.enabled ? theme.textPrimary : theme.textMuted
            anchors.centerIn: parent
        }
    }
}
