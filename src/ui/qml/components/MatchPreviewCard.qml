import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * A card showing a potential match preview for unprocessed ROMs
 * Used to show name-based match suggestions before processing
 */
Rectangle {
    id: root
    
    property string title: ""
    property string system: ""
    property int year: 0
    property string publisher: ""
    property int confidence: 0
    
    signal selected()
    
    height: cardLayout.implicitHeight + 16
    color: mouseArea.containsMouse ? theme.primary + "15" : theme.cardBg
    radius: 6
    border.color: theme.border
    border.width: 1
    
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.selected()
    }
    
    RowLayout {
        id: cardLayout
        anchors.fill: parent
        anchors.margins: 8
        spacing: 10
        
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            
            Label {
                text: root.title
                font.bold: true
                font.pixelSize: 12
                color: theme.textPrimary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            
            Label {
                text: {
                    var parts = [];
                    if (root.system) parts.push(root.system);
                    if (root.year > 0) parts.push(root.year.toString());
                    if (root.publisher) parts.push(root.publisher);
                    return parts.join(" • ");
                }
                font.pixelSize: 10
                color: theme.textMuted
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
        
        // Confidence badge
        Rectangle {
            width: 40
            height: 20
            radius: 4
            color: root.confidence >= 80 ? theme.success :
                   root.confidence >= 50 ? theme.warning : theme.textMuted
            
            Label {
                anchors.centerIn: parent
                text: root.confidence + "%"
                font.pixelSize: 10
                font.bold: true
                color: theme.textLight
            }
        }
    }
}
