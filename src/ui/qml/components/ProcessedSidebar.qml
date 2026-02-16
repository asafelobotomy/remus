import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * Sidebar for processed ROMs
 * Shows cover art, metadata, description, screenshots, file info, and actions
 */
ScrollView {
    id: root
    clip: true
    
    // Basic file data
    property int fileId: -1
    property string filename: ""
    property string filePath: ""
    property string extensions: ""
    property int fileSize: 0
    property int systemId: 0
    property string systemName: "Unknown"  // System display name from database
    
    // Match/metadata data
    property string matchedTitle: ""
    property string matchPublisher: ""
    property string matchDeveloper: ""
    property int matchYear: 0
    property int matchConfidence: 0
    property string matchMethod: ""
    property string description: ""
    property string genre: ""
    property string players: ""
    property string region: ""
    property real rating: 0
    
    // Hash data
    property string crc32: ""
    property string md5: ""
    property string sha1: ""
    
    // Artwork paths
    property string coverArtPath: ""
    property var screenshotPaths: []
    
    // Signals
    signal editMetadataRequested(int fileId)
    signal rematchRequested(int fileId)
    signal openFolderRequested(string path)
    signal screenshotClicked(string path)
    
    // Description expansion
    property bool descriptionExpanded: false
    
    ColumnLayout {
        width: root.width
        spacing: 0
        
        // === Header with Cover Art ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.preferredHeight: headerContent.implicitHeight + 24
            color: theme.cardBg
            radius: 8
            
            ColumnLayout {
                id: headerContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                
                // Cover art
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 140
                    height: 180
                    radius: 6
                    color: theme.border
                    clip: true
                    
                    Image {
                        id: coverImage
                        anchors.fill: parent
                        source: root.coverArtPath ? "file://" + root.coverArtPath : ""
                        fillMode: Image.PreserveAspectFit
                        visible: status === Image.Ready
                    }
                    
                    // Placeholder when no cover art
                    Label {
                        anchors.centerIn: parent
                        text: (root.matchedTitle || root.filename || "?").charAt(0).toUpperCase()
                        font.bold: true
                        font.pixelSize: 56
                        color: theme.textMuted
                        visible: coverImage.status !== Image.Ready
                    }
                }
                
                // Title
                Label {
                    text: root.matchedTitle || root.filename || ""
                    font.bold: true
                    font.pixelSize: 16
                    color: theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                
                // System + Year + Publisher
                Label {
                    text: {
                        var parts = [];
                        if (root.systemName) parts.push(root.systemName);
                        if (root.matchYear > 0) parts.push(root.matchYear.toString());
                        return parts.join(" • ");
                    }
                    font.pixelSize: 13
                    color: theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
                
                Label {
                    text: root.matchPublisher || ""
                    font.pixelSize: 12
                    color: theme.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                    visible: root.matchPublisher !== ""
                }
            }
        }
        
        // === Description Section ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 0
            Layout.preferredHeight: descContent.implicitHeight + 24
            color: theme.cardBg
            radius: 8
            visible: root.description !== ""
            
            ColumnLayout {
                id: descContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                
                Label {
                    text: "Description"
                    font.bold: true
                    font.pixelSize: 13
                    color: theme.textPrimary
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }
                
                Label {
                    text: descriptionExpanded ? root.description : 
                          (root.description.length > 200 ? root.description.substring(0, 200) + "..." : root.description)
                    font.pixelSize: 12
                    color: theme.textSecondary
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                
                Label {
                    text: descriptionExpanded ? "(show less)" : "(show more)"
                    font.pixelSize: 11
                    font.italic: true
                    color: theme.primary
                    visible: root.description.length > 200
                    
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: descriptionExpanded = !descriptionExpanded
                    }
                }
            }
        }
        
        // === Details Section ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 0
            Layout.preferredHeight: detailsContent.implicitHeight + 24
            color: theme.cardBg
            radius: 8
            
            ColumnLayout {
                id: detailsContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                
                Label {
                    text: "Details"
                    font.bold: true
                    font.pixelSize: 13
                    color: theme.textPrimary
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }
                
                GridLayout {
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 4
                    Layout.fillWidth: true
                    
                    Label { text: "Publisher:"; color: theme.textMuted; font.pixelSize: 12 }
                    Label { text: root.matchPublisher || "Unknown"; color: theme.textSecondary; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                    
                    Label { text: "Developer:"; color: theme.textMuted; font.pixelSize: 12; visible: root.matchDeveloper !== "" }
                    Label { text: root.matchDeveloper; color: theme.textSecondary; font.pixelSize: 12; visible: root.matchDeveloper !== ""; Layout.fillWidth: true; elide: Text.ElideRight }
                    
                    Label { text: "Region:"; color: theme.textMuted; font.pixelSize: 12; visible: root.region !== "" }
                    Label { text: root.region; color: theme.textSecondary; font.pixelSize: 12; visible: root.region !== "" }
                    
                    Label { text: "Genre:"; color: theme.textMuted; font.pixelSize: 12; visible: root.genre !== "" }
                    Label { text: root.genre; color: theme.textSecondary; font.pixelSize: 12; visible: root.genre !== ""; Layout.fillWidth: true; elide: Text.ElideRight }
                    
                    Label { text: "Players:"; color: theme.textMuted; font.pixelSize: 12; visible: root.players !== "" }
                    Label { text: root.players; color: theme.textSecondary; font.pixelSize: 12; visible: root.players !== "" }
                    
                    Label { text: "Match:"; color: theme.textMuted; font.pixelSize: 12 }
                    RowLayout {
                        spacing: 4
                        Rectangle {
                            width: confidenceLabel.implicitWidth + 8
                            height: 16
                            radius: 3
                            color: root.matchConfidence >= 80 ? theme.success :
                                   root.matchConfidence >= 50 ? theme.warning : theme.textMuted
                            
                            Label {
                                id: confidenceLabel
                                anchors.centerIn: parent
                                text: root.matchConfidence + "%"
                                font.pixelSize: 10
                                font.bold: true
                                color: theme.textLight
                            }
                        }
                        Label {
                            text: "(" + (root.matchMethod || "none") + ")"
                            font.pixelSize: 11
                            color: theme.textMuted
                        }
                    }
                }
            }
        }
        
        // === Rating Section ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 0
            Layout.preferredHeight: ratingContent.implicitHeight + 24
            color: theme.cardBg
            radius: 8
            visible: root.rating > 0
            
            RowLayout {
                id: ratingContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                
                Label {
                    text: "Rating"
                    font.bold: true
                    font.pixelSize: 13
                    color: theme.textPrimary
                }
                
                Item { Layout.fillWidth: true }
                
                Rectangle {
                    width: ratingLabel.implicitWidth + 16
                    height: 28
                    radius: 6
                    color: theme.primary
                    
                    Label {
                        id: ratingLabel
                        anchors.centerIn: parent
                        text: root.rating.toFixed(1)
                        font.bold: true
                        font.pixelSize: 14
                        color: theme.textLight
                    }
                }
            }
        }
        
        // === Screenshots Section ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 0
            Layout.preferredHeight: screenshotsContent.implicitHeight + 24
            color: theme.cardBg
            radius: 8
            visible: root.screenshotPaths.length > 0
            
            ColumnLayout {
                id: screenshotsContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                
                Label {
                    text: "Screenshots"
                    font.bold: true
                    font.pixelSize: 13
                    color: theme.textPrimary
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }
                
                // Horizontal scrollable screenshots
                ScrollView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 70
                    ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                    ScrollBar.vertical.policy: ScrollBar.AlwaysOff
                    
                    Row {
                        spacing: 8
                        
                        Repeater {
                            model: root.screenshotPaths
                            
                            delegate: Rectangle {
                                width: 80
                                height: 60
                                radius: 4
                                color: theme.border
                                clip: true
                                
                                Image {
                                    anchors.fill: parent
                                    source: "file://" + modelData
                                    fillMode: Image.PreserveAspectCrop
                                }
                                
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.screenshotClicked(modelData)
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // === File Info Section ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 0
            Layout.preferredHeight: fileInfoContent.implicitHeight + 24
            color: theme.cardBg
            radius: 8
            
            ColumnLayout {
                id: fileInfoContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                
                Label {
                    text: "File Info"
                    font.bold: true
                    font.pixelSize: 13
                    color: theme.textPrimary
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }
                
                // Path
                Label {
                    text: root.filePath
                    font.pixelSize: 11
                    color: theme.textMuted
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }
                
                GridLayout {
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 2
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    
                    Label { text: "Size:"; font.pixelSize: 11; color: theme.textMuted }
                    Label { text: formatFileSize(root.fileSize); font.pixelSize: 11; color: theme.textSecondary }
                    
                    Label { text: "CRC32:"; font.pixelSize: 11; color: theme.textMuted; visible: root.crc32 !== "" }
                    RowLayout {
                        spacing: 4
                        visible: root.crc32 !== ""
                        Label { text: root.crc32; font.pixelSize: 11; font.family: "monospace"; color: theme.textSecondary }
                        Label { text: "✓"; font.pixelSize: 10; color: theme.success }
                    }
                }
            }
        }
        
        // === Actions Section ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 0
            Layout.preferredHeight: actionsContent.implicitHeight + 24
            color: theme.cardBg
            radius: 8
            
            ColumnLayout {
                id: actionsContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                
                ThemedButton {
                    text: "Edit Metadata"
                    Layout.fillWidth: true
                    onClicked: root.editMetadataRequested(root.fileId)
                }
                
                ThemedButton {
                    text: "Re-match"
                    Layout.fillWidth: true
                    onClicked: root.rematchRequested(root.fileId)
                }
                
                ThemedButton {
                    text: "Open Folder"
                    Layout.fillWidth: true
                    onClicked: {
                        var folder = root.filePath.substring(0, root.filePath.lastIndexOf("/"));
                        root.openFolderRequested(folder);
                    }
                }
            }
        }
        
        // Spacer
        Item { Layout.fillHeight: true; Layout.minimumHeight: 20 }
    }
    
    // NOTE: System names are now provided via root.systemName property
    // from FileListModel.systemName (SystemNameRole). The hardcoded
    // getSystemName() map has been removed - it contained incorrect
    // mappings and would break when the database schema changes.
    
    function formatFileSize(bytes) {
        if (!bytes || bytes <= 0) return "0 B";
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB";
        if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + " MB";
        return (bytes / 1073741824).toFixed(2) + " GB";
    }
}
