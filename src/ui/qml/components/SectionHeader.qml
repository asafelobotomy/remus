import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * Collapsible section header with count and optional "Select All" checkbox
 * Used for Unprocessed/Processed sections in LibraryView
 */
Rectangle {
    id: root
    
    property string title: "Section"
    property int count: 0
    property bool expanded: true
    property bool showSelectAll: false
    property bool selectAllChecked: false
    
    signal toggleExpanded()
    signal selectAllToggled(bool checked)
    
    height: 40
    color: theme.cardBg
    
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8
        
        // Expand/collapse arrow
        Label {
            text: root.expanded ? "▼" : "▶"
            font.pixelSize: 12
            color: theme.textSecondary
            
            MouseArea {
                anchors.fill: parent
                anchors.margins: -8
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleExpanded()
            }
        }
        
        // Section title with count
        Label {
            text: root.title + " (" + root.count + ")"
            font.bold: true
            font.pixelSize: 14
            color: theme.textPrimary
            
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleExpanded()
            }
        }
        
        Item { Layout.fillWidth: true }
        
        // Select All checkbox (optional)
        ThemedCheckBox {
            visible: root.showSelectAll && root.expanded
            text: "Select All"
            checked: root.selectAllChecked
            onCheckedChanged: {
                if (checked !== root.selectAllChecked) {
                    root.selectAllToggled(checked)
                }
            }
        }
    }
    
    // Bottom border
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: theme.border
    }
}
