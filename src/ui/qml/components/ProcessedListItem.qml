import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * List item for processed ROMs
 * Shows: Thumbnail + title + system + year + match indicator
 */
ItemDelegate {
    id: root
    
    property int fileId: 0
    property string title: ""
    property string system: ""
    property int year: 0
    property int matchConfidence: 0
    property string thumbnailPath: ""
    property bool isHighlighted: false
    
    signal itemClicked(int fileId)
    
    width: parent ? parent.width : 300
    height: 56
    
    background: Rectangle {
        color: root.isHighlighted ? theme.primary + "20" : 
               root.hovered ? theme.cardBg : "transparent"
    }
    
    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12
        
        // Thumbnail or placeholder
        Rectangle {
            width: 40
            height: 40
            radius: 4
            color: theme.cardBg
            border.color: theme.border
            border.width: 1
            
            Image {
                id: thumbnail
                anchors.fill: parent
                anchors.margins: 2
                source: root.thumbnailPath
                fillMode: Image.PreserveAspectFit
                visible: status === Image.Ready
            }
            
            // Placeholder when no image
            Label {
                anchors.centerIn: parent
                text: root.title.charAt(0).toUpperCase()
                font.bold: true
                font.pixelSize: 18
                color: theme.textMuted
                visible: thumbnail.status !== Image.Ready
            }
        }
        
        // Title and metadata
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            
            Label {
                text: root.title
                font.bold: true
                font.pixelSize: 13
                color: theme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            
            RowLayout {
                spacing: 8
                
                Label {
                    text: root.system
                    font.pixelSize: 11
                    color: theme.textSecondary
                    visible: root.system !== ""
                }
                
                Label {
                    text: "•"
                    font.pixelSize: 11
                    color: theme.textMuted
                    visible: root.system !== "" && root.year > 0
                }
                
                Label {
                    text: root.year > 0 ? root.year.toString() : ""
                    font.pixelSize: 11
                    color: theme.textSecondary
                    visible: root.year > 0
                }
            }
        }
        
        // Match indicator
        Rectangle {
            width: 24
            height: 24
            radius: 12
            color: root.matchConfidence >= 80 ? theme.success :
                   root.matchConfidence >= 50 ? theme.warning : theme.info
            visible: root.matchConfidence > 0
            
            Label {
                anchors.centerIn: parent
                text: "✓"
                font.bold: true
                font.pixelSize: 12
                color: theme.textLight
            }
        }
    }
    
    onClicked: root.itemClicked(root.fileId)
}
