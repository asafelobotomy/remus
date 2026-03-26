import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * List item for unprocessed ROMs
 * Shows: Checkbox + [?] icon + filename + file extensions
 */
ItemDelegate {
    id: root
    
    property int fileId: 0
    property string filename: ""
    property string extensions: ""
    property string archiveExtension: ""
    property bool isSelected: false
    property bool isHighlighted: false
    
    signal selectionToggled(int fileId, bool selected)
    signal itemClicked(int fileId)
    
    width: parent ? parent.width : 300
    height: 48
    
    background: Rectangle {
        color: root.isHighlighted ? theme.primary + "20" : 
               root.hovered ? theme.cardBg : "transparent"
    }
    
    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 10
        
        // Checkbox
        ThemedCheckBox {
            checked: root.isSelected
            onClicked: {
                root.selectionToggled(root.fileId, !root.isSelected)
            }
        }
        
        // Unknown/unprocessed icon
        Rectangle {
            width: 32
            height: 32
            radius: 4
            color: theme.textMuted
            
            Label {
                anchors.centerIn: parent
                text: "?"
                font.bold: true
                font.pixelSize: 16
                color: theme.textLight
            }
        }
        
        // Filename
        Label {
            text: root.filename
            font.pixelSize: 13
            color: theme.textPrimary
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        
        // Extension badges row
        Row {
            spacing: 4
            
            // File extension badges
            Repeater {
                model: root.extensions ? root.extensions.split(",") : []
                
                Rectangle {
                    width: extBadgeLabel.implicitWidth + 10
                    height: 18
                    radius: 3
                    color: theme.border
                    
                    Label {
                        id: extBadgeLabel
                        anchors.centerIn: parent
                        text: "." + modelData.trim().toUpperCase()
                        font.pixelSize: 9
                        font.bold: true
                        color: theme.textSecondary
                    }
                }
            }
            
            // Archive extension badge (if from archive)
            Rectangle {
                visible: root.archiveExtension !== ""
                width: archiveBadgeLabel.implicitWidth + 10
                height: 18
                radius: 3
                color: theme.primary + "40"
                
                Label {
                    id: archiveBadgeLabel
                    anchors.centerIn: parent
                    text: root.archiveExtension.toUpperCase()
                    font.pixelSize: 9
                    font.bold: true
                    color: theme.primary
                }
            }
        }
    }
    
    onClicked: root.itemClicked(root.fileId)
}
