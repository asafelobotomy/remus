import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "components"

Page {
    title: "Match Review"
    background: Rectangle { color: theme.mainBg }
    palette.window: theme.mainBg
    palette.windowText: theme.textPrimary
    palette.base: theme.mainBg
    palette.text: theme.textPrimary
    palette.button: theme.mainBg
    palette.buttonText: theme.textPrimary
    palette.highlight: theme.primary
    palette.highlightedText: theme.textLight
    palette.placeholderText: theme.textMuted
    
    header: ToolBar {
        background: Rectangle { color: theme.cardBg }
        RowLayout {
            anchors.fill: parent
            
            Label {
                text: "Match Review"
                font.pixelSize: 18
                font.bold: true
                color: theme.textPrimary
            }
            
            Item { Layout.fillWidth: true }
            
            Label {
                text: "Filter:"
                font.bold: true
                color: theme.textPrimary
            }
            
            ThemedComboBox {
                id: confidenceFilter
                model: ["All Matches", "High Confidence", "Medium Confidence", "Low Confidence"]
                Layout.preferredWidth: 180
                onCurrentTextChanged: {
                    if (currentText === "All Matches") matchListModel.confidenceFilter = "all";
                    else if (currentText === "High Confidence") matchListModel.confidenceFilter = "high";
                    else if (currentText === "Medium Confidence") matchListModel.confidenceFilter = "medium";
                    else if (currentText === "Low Confidence") matchListModel.confidenceFilter = "low";
                }
            }
            
            ThemedButton {
                text: "Start Matching"
                icon.name: "system-search"
                enabled: !matchController.matching
                isPrimary: true
                onClicked: matchController.startMatching()
            }
            
            ThemedButton {
                text: "Refresh"
                icon.name: "view-refresh"
                onClicked: matchListModel.refresh()
            }
        }
    }
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: theme.mainBg
            
            ListView {
                id: matchListView
                anchors.fill: parent
                model: matchListModel
                clip: true
                spacing: 5
                
                delegate: ItemDelegate {
                    width: matchListView.width
                    height: 120
                    
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 5
                        color: theme.cardBg
                        radius: 5
                        border.color: theme.border
                        border.width: 1
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 15
                            spacing: 15
                            
                            // Left side - File info
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                spacing: 5
                                
                                Label {
                                    text: "File: " + model.filename
                                    font.bold: true
                                    color: theme.textPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                
                                Rectangle {
                                    width: 2
                                    height: 30
                                    color: theme.border
                                }
                                
                                Label {
                                    text: "Match: " + model.title
                                    font.pixelSize: 14
                                    color: theme.textPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                
                                RowLayout {
                                    spacing: 10
                                    
                                    Label {
                                        text: "Platform: " + model.platform
                                        font.pixelSize: 11
                                        color: theme.textSecondary
                                    }
                                    
                                    Label {
                                        text: model.region ? "Region: " + model.region : ""
                                        font.pixelSize: 11
                                        color: theme.textSecondary
                                        visible: model.region
                                    }
                                    
                                    Label {
                                        text: model.year ? "Year: " + model.year : ""
                                        font.pixelSize: 11
                                        color: theme.textSecondary
                                        visible: model.year
                                    }
                                }
                                
                                Label {
                                    text: "Method: " + model.matchMethod
                                    font.pixelSize: 10
                                    color: theme.textMuted
                                }
                            }
                            
                            // Right side - Confidence and actions
                            ColumnLayout {
                                Layout.preferredWidth: 150
                                Layout.fillHeight: true
                                spacing: 10
                                
                                Rectangle {
                                    Layout.alignment: Qt.AlignHCenter
                                    width: 80
                                    height: 80
                                    radius: 40
                                    color: model.confidenceColor
                                    
                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 2
                                        
                                        Label {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: Math.round(model.confidence) + "%"
                                            font.pixelSize: 20
                                            font.bold: true
                                            color: theme.textLight
                                        }
                                        
                                        Label {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: model.confidenceLabel
                                            font.pixelSize: 10
                                            color: theme.textLight
                                        }
                                    }
                                }
                                
                                Item { Layout.fillHeight: true }
                                
                                ThemedButton {
                                    Layout.fillWidth: true
                                    text: "Confirm"
                                    icon.name: "emblem-ok"
                                    isPrimary: true
                                    onClicked: matchListModel.confirmMatch(index)
                                }
                                
                                ThemedButton {
                                    Layout.fillWidth: true
                                    text: "Reject"
                                    icon.name: "edit-delete"
                                    onClicked: matchListModel.rejectMatch(index)
                                }
                            }
                        }
                    }
                }
                
                ScrollBar.vertical: ScrollBar {}
                
                Label {
                    anchors.centerIn: parent
                    text: matchController.matching ? "Matching in progress...\n\nPlease wait" : "No matches to review\n\nClick 'Start Matching' to find metadata"
                    visible: matchListModel.count === 0
                    horizontalAlignment: Text.AlignHCenter
                    color: theme.textMuted
                    font.pixelSize: 14
                }
            }
        }
    }
}
