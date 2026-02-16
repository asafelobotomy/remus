import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * @brief Pipeline status bar showing overall workflow progress
 * 
 * Displays counts for each stage: Scanned, Extracted, Hashed, Matched
 * with an overall progress bar.
 */
Rectangle {
    id: root
    height: 50
    color: theme.cardBg
    
    // Stats properties - bind to model aggregates
    property int totalGames: 0
    property int extractedCount: 0
    property int hashedCount: 0
    property int matchedCount: 0
    property int needsChdCount: 0
    
    // Computed progress (0-100)
    property int overallProgress: {
        if (totalGames === 0) return 0;
        // Weight: extracted (25%), hashed (25%), matched (50%)
        var extractPct = extractedCount / totalGames * 25;
        var hashPct = hashedCount / totalGames * 25;
        var matchPct = matchedCount / totalGames * 50;
        return Math.round(extractPct + hashPct + matchPct);
    }
    
    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 20
        
        // Progress bar
        ColumnLayout {
            spacing: 2
            Layout.preferredWidth: 150
            
            Label {
                text: "Pipeline Progress"
                font.pixelSize: 10
                font.bold: true
                color: theme.textMuted
            }
            
            ThemedProgressBar {
                Layout.fillWidth: true
                value: overallProgress / 100.0
            }
        }
        
        // Separator
        Rectangle {
            width: 1
            Layout.fillHeight: true
            Layout.topMargin: 5
            Layout.bottomMargin: 5
            color: theme.border
        }
        
        // Stats row
        Row {
            spacing: 25
            
            // Scanned
            StatItem {
                label: "Scanned"
                value: totalGames
                iconColor: theme.primary
            }
            
            // Extracted
            StatItem {
                label: "Extracted"
                value: extractedCount
                iconColor: theme.info
            }
            
            // Hashed
            StatItem {
                label: "Hashed"
                value: hashedCount
                iconColor: theme.warning
            }
            
            // Matched
            StatItem {
                label: "Matched"
                value: matchedCount
                iconColor: theme.success
            }
            
            // Needs CHD (optional)
            StatItem {
                label: "Needs CHD"
                value: needsChdCount
                iconColor: theme.textMuted
                visible: needsChdCount > 0
            }
        }
        
        Item { Layout.fillWidth: true }
        
        // Overall percentage
        Label {
            text: overallProgress + "%"
            font.pixelSize: 24
            font.bold: true
            color: overallProgress >= 80 ? theme.success : 
                   overallProgress >= 40 ? theme.warning : theme.textMuted
        }
    }
    
    // StatItem component
    component StatItem: Column {
        property string label: ""
        property int value: 0
        property color iconColor: theme.primary
        
        spacing: 2
        
        Row {
            spacing: 5
            
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: iconColor
                anchors.verticalCenter: parent.verticalCenter
            }
            
            Label {
                text: value
                font.pixelSize: 16
                font.bold: true
                color: theme.textPrimary
            }
        }
        
        Label {
            text: label
            font.pixelSize: 10
            font.bold: true
            color: theme.textMuted
        }
    }
}
