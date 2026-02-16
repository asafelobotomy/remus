import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * @brief Status badge component showing workflow state
 * 
 * Visual indicator for extraction/CHD/hash/match states
 */
Rectangle {
    id: root
    
    property int workflowState: 0  // WorkflowState enum value
    property string label: ""
    property bool compact: false
    
    // WorkflowState enum values (from C++)
    readonly property int stateNotApplicable: 0
    readonly property int stateNeedsAction: 1
    readonly property int stateInProgress: 2
    readonly property int stateComplete: 3
    readonly property int stateFailed: 4
    readonly property int stateSkipped: 5
    
    width: compact ? 20 : implicitWidth
    height: compact ? 20 : 24
    radius: compact ? 10 : 4
    
    implicitWidth: compact ? 20 : (row.implicitWidth + 12)
    
    color: {
        switch (workflowState) {
            case stateNotApplicable: return "transparent"
            case stateNeedsAction: return theme.warning + "40"  // 25% opacity
            case stateInProgress: return theme.info + "60"
            case stateComplete: return theme.success + "40"
            case stateFailed: return theme.danger + "40"
            case stateSkipped: return theme.textMuted + "40"
            default: return "transparent"
        }
    }
    
    border.width: workflowState !== stateNotApplicable ? 1 : 0
    border.color: {
        switch (workflowState) {
            case stateNeedsAction: return theme.warning
            case stateInProgress: return theme.info
            case stateComplete: return theme.success
            case stateFailed: return theme.danger
            case stateSkipped: return theme.textMuted
            default: return "transparent"
        }
    }
    
    visible: workflowState !== stateNotApplicable
    
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 4
        
        // Icon
        Label {
            text: {
                switch (workflowState) {
                    case stateNeedsAction: return "○"
                    case stateInProgress: return "◐"
                    case stateComplete: return "✓"
                    case stateFailed: return "✗"
                    case stateSkipped: return "—"
                    default: return ""
                }
            }
            font.pixelSize: compact ? 10 : 12
            font.bold: true
            color: {
                switch (workflowState) {
                    case stateNeedsAction: return theme.warning
                    case stateInProgress: return theme.info
                    case stateComplete: return theme.success
                    case stateFailed: return theme.danger
                    case stateSkipped: return theme.textMuted
                    default: return theme.textMuted
                }
            }
        }
        
        // Label (only in non-compact mode)
        Label {
            text: label
            font.pixelSize: 10
            font.bold: true
            visible: !compact && label !== ""
            color: {
                switch (workflowState) {
                    case stateNeedsAction: return theme.warning
                    case stateInProgress: return theme.info
                    case stateComplete: return theme.success
                    case stateFailed: return theme.danger
                    case stateSkipped: return theme.textMuted
                    default: return theme.textMuted
                }
            }
        }
    }
}
