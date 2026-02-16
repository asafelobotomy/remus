import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * @brief Detail panel for selected ROM showing workflow and actions
 * 
 * Shows file info, workflow progress, and action buttons for the 
 * selected entry in the file list.
 */
Rectangle {
    id: root
    color: theme.cardBg
    
    // Selected item properties - bind from ListView
    property string gameName: ""
    property string filePath: ""
    property string extensions: ""
    property int fileCount: 1
    property int fileSize: 0
    property int systemId: 0
    
    // Workflow states (0=N/A, 1=NeedsAction, 2=InProgress, 3=Complete, 4=Failed, 5=Skipped)
    property int extractionState: 0
    property int chdState: 0
    property int hashState: 0
    property int matchState: 0
    
    // Source info
    property bool isInsideArchive: false
    property string archivePath: ""
    property bool isChdCandidate: false
    property bool isAlreadyChd: false
    
    // Match info
    property int matchConfidence: 0
    property string matchMethod: ""
    property string matchedTitle: ""
    property string matchPublisher: ""
    property int matchYear: 0
    property bool matchConfirmed: false
    property bool matchRejected: false
    
    // Selected file IDs for actions
    property string allFileIds: ""
    property int primaryFileId: 0
    
    // Signals for actions
    signal extractRequested(string archivePath)
    signal convertToChdRequested(int fileId)
    signal hashRequested(int fileId)
    signal matchRequested(int fileId)
    signal confirmMatchRequested(int fileId)
    signal rejectMatchRequested(int fileId)
    
    // Empty state
    property bool hasSelection: gameName !== ""
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 15
        
        // Header
        Label {
            text: hasSelection ? gameName : "Select a ROM"
            font.pixelSize: 16
            font.bold: true
            color: theme.textPrimary
            elide: Text.ElideRight
            Layout.fillWidth: true
        }
        
        // File info section
        Rectangle {
            Layout.fillWidth: true
            height: infoColumn.implicitHeight + 20
            color: theme.mainBg
            radius: 8
            visible: hasSelection
            
            ColumnLayout {
                id: infoColumn
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6
                
                InfoRow { label: "Files:"; value: fileCount + " (" + extensions.toUpperCase() + ")" }
                InfoRow { label: "Size:"; value: formatFileSize(fileSize) }
                InfoRow { label: "Path:"; value: filePath; elideValue: true }
                InfoRow { 
                    label: "Source:"; 
                    value: isInsideArchive ? "Archive: " + archivePath.split("/").pop() : "Direct" 
                    visible: isInsideArchive
                }
            }
        }
        
        // Workflow progress section
        Rectangle {
            Layout.fillWidth: true
            height: workflowColumn.implicitHeight + 20
            color: theme.mainBg
            radius: 8
            visible: hasSelection
            
            ColumnLayout {
                id: workflowColumn
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8
                
                Label {
                    text: "Workflow Progress"
                    font.bold: true
                    font.pixelSize: 12
                    color: theme.textPrimary
                }
                
                // Extraction step
                WorkflowStep {
                    stepNumber: 1
                    stepName: "Extracted"
                    workflowState: extractionState
                    actionText: "Extract"
                    showAction: extractionState === 1  // NeedsAction
                    onActionClicked: root.extractRequested(archivePath)
                }
                
                // CHD step
                WorkflowStep {
                    stepNumber: 2
                    stepName: "CHD Converted"
                    workflowState: chdState
                    actionText: "Convert"
                    showAction: chdState === 1 && isChdCandidate
                    infoText: isAlreadyChd ? "Already CHD" : (isChdCandidate ? "Optional" : "N/A")
                    onActionClicked: root.convertToChdRequested(primaryFileId)
                }
                
                // Hash step
                WorkflowStep {
                    stepNumber: 3
                    stepName: "Hashed"
                    workflowState: hashState
                    actionText: "Hash"
                    showAction: hashState === 1
                    onActionClicked: root.hashRequested(primaryFileId)
                }
                
                // Match step
                WorkflowStep {
                    stepNumber: 4
                    stepName: "Matched"
                    workflowState: matchState
                    actionText: "Match"
                    showAction: matchState === 1 && hashState === 3  // Needs match and has hash
                    onActionClicked: root.matchRequested(primaryFileId)
                }
            }
        }
        
        // Match result section (if matched)
        Rectangle {
            Layout.fillWidth: true
            height: matchColumn.implicitHeight + 20
            color: matchConfidence >= 80 ? theme.success + "20" : 
                   matchConfidence >= 50 ? theme.warning + "20" : theme.danger + "20"
            radius: 8
            visible: hasSelection && matchedTitle !== ""
            border.width: 1
            border.color: matchConfidence >= 80 ? theme.success : 
                          matchConfidence >= 50 ? theme.warning : theme.danger
            
            ColumnLayout {
                id: matchColumn
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6
                
                RowLayout {
                    Layout.fillWidth: true
                    
                    Label {
                        text: "Match Result"
                        font.bold: true
                        font.pixelSize: 12
                        color: theme.textPrimary
                    }
                    
                    Item { Layout.fillWidth: true }
                    
                    Label {
                        text: matchConfidence + "%"
                        font.bold: true
                        font.pixelSize: 14
                        color: matchConfidence >= 80 ? theme.success : 
                               matchConfidence >= 50 ? theme.warning : theme.danger
                    }
                }
                
                Label {
                    text: matchedTitle
                    font.pixelSize: 14
                    font.bold: true
                    color: theme.textPrimary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                
                InfoRow { 
                    label: "Publisher:"; 
                    value: matchPublisher || "Unknown"
                    visible: matchPublisher !== ""
                }
                InfoRow { 
                    label: "Year:"; 
                    value: matchYear > 0 ? matchYear.toString() : "Unknown"
                    visible: matchYear > 0
                }
                InfoRow { 
                    label: "Method:"; 
                    value: matchMethod 
                }
                
                // Confirm/Reject buttons
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    visible: !matchConfirmed && !matchRejected
                    
                    ThemedButton {
                        text: "Confirm"
                        isPrimary: true
                        Layout.fillWidth: true
                        onClicked: root.confirmMatchRequested(primaryFileId)
                    }
                    
                    ThemedButton {
                        text: "Reject"
                        Layout.fillWidth: true
                        onClicked: root.rejectMatchRequested(primaryFileId)
                    }
                }
                
                // Status label if already confirmed/rejected
                Label {
                    text: matchConfirmed ? "✓ Confirmed" : "✗ Rejected"
                    font.bold: true
                    color: matchConfirmed ? theme.success : theme.danger
                    visible: matchConfirmed || matchRejected
                }
            }
        }
        
        Item { Layout.fillHeight: true }
        
        // Quick actions at bottom
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: hasSelection
            
            Label {
                text: "Quick Actions"
                font.bold: true
                font.pixelSize: 12
                color: theme.textMuted
            }
            
            ThemedButton {
                text: "Process All Steps"
                Layout.fillWidth: true
                isPrimary: true
                enabled: extractionState === 1 || hashState === 1 || matchState === 1
                onClicked: {
                    console.log("Process All Steps clicked");
                    console.log("  extractionState:", extractionState, "isInsideArchive:", isInsideArchive);
                    console.log("  hashState:", hashState);
                    console.log("  matchState:", matchState);
                    console.log("  primaryFileId:", primaryFileId);
                    console.log("  archivePath:", archivePath);
                    
                    // Trigger extraction first if needed
                    if (extractionState === 1 && isInsideArchive && archivePath) {
                        console.log("Triggering extraction...");
                        root.extractRequested(archivePath);
                        return;  // Extraction will trigger rescan which will update states
                    }
                    
                    // Hash if needed
                    if (hashState === 1 && primaryFileId > 0) {
                        console.log("Triggering hash...");
                        root.hashRequested(primaryFileId);
                        return;  // Hash will refresh model
                    }
                    
                    // Match if needed
                    if (matchState === 1 && primaryFileId > 0) {
                        console.log("Triggering match...");
                        root.matchRequested(primaryFileId);
                    }
                }
            }
            
            ThemedButton {
                text: "Open in File Manager"
                Layout.fillWidth: true
                onClicked: Qt.openUrlExternally("file://" + filePath.substring(0, filePath.lastIndexOf("/")))
            }
        }
    }
    
    // InfoRow component
    component InfoRow: RowLayout {
        property string label: ""
        property string value: ""
        property bool elideValue: false
        
        Layout.fillWidth: true
        
        Label {
            text: label
            font.pixelSize: 11
            font.bold: true
            color: theme.textMuted
            Layout.preferredWidth: 60
        }
        
        Label {
            text: value
            font.pixelSize: 11
            color: theme.textSecondary
            elide: elideValue ? Text.ElideMiddle : Text.ElideNone
            Layout.fillWidth: true
        }
    }
    
    // WorkflowStep component
    component WorkflowStep: RowLayout {
        property int stepNumber: 1
        property string stepName: ""
        property int workflowState: 0
        property string actionText: ""
        property bool showAction: false
        property string infoText: ""
        
        signal actionClicked()
        
        Layout.fillWidth: true
        spacing: 8
        
        // Step number circle
        Rectangle {
            width: 20
            height: 20
            radius: 10
            color: workflowState === 3 ? theme.success : 
                   workflowState === 0 ? theme.textMuted + "40" : theme.warning
            
            Label {
                anchors.centerIn: parent
                text: workflowState === 3 ? "✓" : stepNumber.toString()
                font.pixelSize: 10
                font.bold: true
                color: theme.textLight
            }
        }
        
        // Step name
        Label {
            text: stepName
            font.pixelSize: 11
            color: workflowState === 3 ? theme.success : 
                   workflowState === 0 ? theme.textMuted : theme.textPrimary
            Layout.fillWidth: true
        }
        
        // Info text or action button
        Label {
            text: infoText
            font.pixelSize: 10
            color: theme.textMuted
            visible: !showAction && infoText !== "" && workflowState === 0
        }
        
        WorkflowBadge {
            workflowState: parent.workflowState
            compact: true
            visible: !showAction && parent.workflowState !== 0
        }
        
        ThemedButton {
            text: actionText
            visible: showAction
            onClicked: parent.actionClicked()
        }
    }
    
    function formatFileSize(bytes) {
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB";
        if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + " MB";
        return (bytes / 1073741824).toFixed(2) + " GB";
    }
}
