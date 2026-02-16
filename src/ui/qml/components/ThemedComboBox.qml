import QtQuick 2.15
import QtQuick.Controls 2.15

// Themed ComboBox that overrides KDE Breeze defaults with explicit styling
ComboBox {
    id: control
    
    // Explicit background overriding system style
    background: Rectangle {
        implicitWidth: 180
        implicitHeight: 40
        color: control.enabled ? theme.mainBg : theme.cardBg
        border.color: control.pressed || control.popup.visible ? theme.primary : theme.border
        border.width: control.pressed || control.popup.visible ? 2 : 1
        radius: 4
    }
    
    // Explicit content item - KDE Breeze ignores palette.buttonText
    contentItem: Text {
        text: control.displayText
        font: control.font
        color: control.enabled ? theme.textPrimary : theme.textMuted
        verticalAlignment: Text.AlignVCenter
        leftPadding: 12
        rightPadding: control.indicator.width + 8
        elide: Text.ElideRight
    }
    
    // Dropdown indicator arrow
    indicator: Canvas {
        id: canvas
        x: control.width - width - 8
        y: (control.height - height) / 2
        width: 12
        height: 8
        contextType: "2d"
        
        Connections {
            target: theme
            function onThemeModeChanged() { canvas.requestPaint() }
        }
        
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.moveTo(0, 0)
            ctx.lineTo(width, 0)
            ctx.lineTo(width / 2, height)
            ctx.closePath()
            ctx.fillStyle = theme.textPrimary
            ctx.fill()
        }
    }
    
    // Popup styling for dropdown menu
    popup: Popup {
        y: control.height + 2
        width: control.width
        implicitHeight: contentItem.implicitHeight
        padding: 1
        
        background: Rectangle {
            color: theme.cardBg
            border.color: theme.border
            border.width: 1
            radius: 4
        }
        
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
    
    // Delegate for dropdown items
    delegate: ItemDelegate {
        width: control.width
        height: 36
        
        background: Rectangle {
            color: highlighted ? theme.primary : "transparent"
            radius: 2
        }
        
        contentItem: Text {
            text: control.textRole ? (Array.isArray(control.model) ? modelData : model[control.textRole]) : modelData
            font: control.font
            color: highlighted ? theme.textLight : theme.textPrimary
            verticalAlignment: Text.AlignVCenter
            leftPadding: 12
            elide: Text.ElideRight
        }
        
        highlighted: control.highlightedIndex === index
    }
}
