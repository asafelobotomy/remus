import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

/**
 * Sidebar for unprocessed ROMs
 * Shows archive contents, possible matches, hash tools, and actions
 */
ScrollView {
    id: root
    clip: true
    
    // Data properties
    property int fileId: -1
    property string filename: ""
    property string filePath: ""
    property string extensions: ""
    property int fileSize: 0
    property int systemId: 0
    property string systemName: "Unknown"  // System display name from database
    property bool isInsideArchive: false
    property string archivePath: ""
    property string crc32: ""
    property string md5: ""
    property string sha1: ""
    property bool hasHashes: false
    
    // Possible matches (populated by quick search)
    property var possibleMatches: []
    property var currentMatch: null  // Current match during processing
    
    // Enhanced metadata from processing
    property string gameDescription: ""
    property string coverArtUrl: ""
    property string systemLogoUrl: ""
    property string screenshotUrl: ""
    property string titleScreenUrl: ""
    property real gameRating: 0.0
    property string ratingSource: ""
    
    // Signals
    signal calculateHashRequested(int fileId)
    signal removeRequested(int fileId)
    signal previewRequested(string path)
    signal matchSelected(var matchData)
    
    // Connect to ProcessingController signals for real-time updates
    Connections {
        target: processingController
        
        function onHashCalculated(fileId, crc32Hash, md5Hash, sha1Hash) {
            // Only update if this is the currently displayed file
            if (fileId === root.fileId) {
                root.crc32 = crc32Hash
                root.md5 = md5Hash
                root.sha1 = sha1Hash
                root.hasHashes = true
            }
        }
        
        function onMatchFound(fileId, title, publisher, year, confidence, matchMethod) {
            // Only update if this is the currently displayed file
            if (fileId === root.fileId) {
                root.currentMatch = {
                    title: title,
                    publisher: publisher,
                    year: year,
                    confidence: confidence,
                    matchMethod: matchMethod
                }
                // Add to possible matches if not already there
                var matchExists = root.possibleMatches.some(function(m) {
                    return m.title === title && m.publisher === publisher
                })
                if (!matchExists) {
                    root.possibleMatches.push(root.currentMatch)
                }
            }
        }
        
        function onMetadataUpdated(fileId, description, coverArtUrl, systemLogoUrl, 
                                   screenshotUrl, titleScreenUrl, rating, ratingSource) {
            // Only update if this is the currently displayed file
            if (fileId === root.fileId) {
                root.gameDescription = description || ""
                root.coverArtUrl = coverArtUrl || ""
                root.systemLogoUrl = systemLogoUrl || ""
                root.screenshotUrl = screenshotUrl || ""
                root.titleScreenUrl = titleScreenUrl || ""
                root.gameRating = rating || 0.0
                root.ratingSource = ratingSource || ""
            }
        }
    }
    
    ColumnLayout {
        width: root.width
        spacing: 0
        
        // === Header Section ===
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
                spacing: 8
                
                RowLayout {
                    spacing: 12
                    
                    // Unknown icon
                    Rectangle {
                        width: 48
                        height: 48
                        radius: 6
                        color: theme.textMuted
                        
                        Label {
                            anchors.centerIn: parent
                            text: "?"
                            font.bold: true
                            font.pixelSize: 24
                            color: theme.textLight
                        }
                    }
                    
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        
                        Label {
                            text: root.filename
                            font.bold: true
                            font.pixelSize: 14
                            color: theme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        
                        Label {
                            text: "Unprocessed"
                            font.pixelSize: 12
                            color: theme.warning
                            font.bold: true
                        }
                    }
                }
            }
        }
        
        // === Archive Contents Section ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 0
            Layout.preferredHeight: archiveContent.implicitHeight + 24
            color: theme.cardBg
            radius: 8
            visible: root.isInsideArchive
            
            ColumnLayout {
                id: archiveContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                
                Label {
                    text: "Archive Contents"
                    font.bold: true
                    font.pixelSize: 13
                    color: theme.textPrimary
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }
                
                RowLayout {
                    spacing: 8
                    
                    Label {
                        text: "├──"
                        font.family: "monospace"
                        font.pixelSize: 11
                        color: theme.textMuted
                    }
                    
                    Label {
                        text: root.filename + (root.extensions ? "." + root.extensions.split("/")[0] : "")
                        font.pixelSize: 12
                        color: theme.textSecondary
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
                
                Label {
                    text: "└── (" + formatFileSize(root.fileSize) + " total)"
                    font.family: "monospace"
                    font.pixelSize: 11
                    color: theme.textMuted
                }
            }
        }
        
        // === Possible Matches Section ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 0
            Layout.preferredHeight: matchesContent.implicitHeight + 24
            color: theme.cardBg
            radius: 8
            visible: root.possibleMatches.length > 0
            
            ColumnLayout {
                id: matchesContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                
                Label {
                    text: "Possible Matches"
                    font.bold: true
                    font.pixelSize: 13
                    color: theme.textPrimary
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }
                
                Repeater {
                    model: root.possibleMatches.slice(0, 3) // Show max 3
                    
                    delegate: MatchPreviewCard {
                        Layout.fillWidth: true
                        title: modelData.title || ""
                        system: modelData.system || ""
                        year: modelData.year || 0
                        publisher: modelData.publisher || ""
                        confidence: modelData.confidence || 0
                        onSelected: root.matchSelected(modelData)
                    }
                }
                
                Label {
                    text: "(matches based on filename)"
                    font.pixelSize: 10
                    font.italic: true
                    color: theme.textMuted
                    Layout.topMargin: 4
                }
            }
        }
        
        // === Current Match During Processing (Real-Time) ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 0
            Layout.preferredHeight: currentMatchContent.implicitHeight + 24
            color: theme.cardBg
            radius: 8
            visible: root.currentMatch !== null
            border.color: theme.success
            border.width: 2
            
            ColumnLayout {
                id: currentMatchContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                
                RowLayout {
                    spacing: 8
                    
                    Label {
                        text: "Match Found"
                        font.bold: true
                        font.pixelSize: 13
                        color: theme.success
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: "(processing...)"
                        font.pixelSize: 11
                        font.italic: true
                        color: theme.textMuted
                    }
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }
                
                Label {
                    text: root.currentMatch ? root.currentMatch.title : ""
                    font.bold: true
                    font.pixelSize: 12
                    color: theme.textPrimary
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                
                GridLayout {
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 6
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    
                    Label {
                        text: "Publisher:"
                        font.pixelSize: 10
                        color: theme.textMuted
                    }
                    
                    Label {
                        text: root.currentMatch ? root.currentMatch.publisher : ""
                        font.pixelSize: 10
                        color: theme.textSecondary
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: "Year:"
                        font.pixelSize: 10
                        color: theme.textMuted
                    }
                    
                    Label {
                        text: root.currentMatch ? root.currentMatch.year : ""
                        font.pixelSize: 10
                        color: theme.textSecondary
                    }
                    
                    Label {
                        text: "Confidence:"
                        font.pixelSize: 10
                        color: theme.textMuted
                    }
                    
                    RowLayout {
                        spacing: 4
                        
                        Rectangle {
                            width: 40
                            height: 16
                            radius: 3
                            color: {
                                var conf = root.currentMatch ? root.currentMatch.confidence : 0
                                if (conf >= 90) return theme.success
                                if (conf >= 70) return theme.warning
                                return theme.danger
                            }
                            
                            Label {
                                anchors.centerIn: parent
                                text: (root.currentMatch ? root.currentMatch.confidence : 0) + "%"
                                font.pixelSize: 9
                                font.bold: true
                                color: theme.textLight
                            }
                        }
                        
                        Label {
                            text: root.currentMatch ? (root.currentMatch.matchMethod === "hash" ? "via Hash" : "via Name") : ""
                            font.pixelSize: 10
                            color: theme.textMuted
                            font.italic: true
                        }
                    }
                }
            }
        }
        
        // === Game Details Section (Enhanced Metadata) ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 0
            color: theme.cardBg
            radius: 8
            visible: root.coverArtUrl !== "" || root.gameDescription !== "" || root.screenshotUrl !== ""
            Layout.preferredHeight: gameDetailsContent.implicitHeight + 24
            
            ColumnLayout {
                id: gameDetailsContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12
                
                Label {
                    text: "Game Details"
                    font.bold: true
                    font.pixelSize: 13
                    color: theme.textPrimary
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }
                
                // Cover Art
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 200
                    color: theme.bgSecondary
                    radius: 6
                    visible: root.coverArtUrl !== ""
                    
                    Image {
                        anchors.fill: parent
                        anchors.margins: 4
                        source: root.coverArtUrl
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        asynchronous: true
                        
                        BusyIndicator {
                            anchors.centerIn: parent
                            running: parent.status === Image.Loading
                            visible: running
                        }
                    }
                }
                
                // System Logo
                Image {
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 40
                    source: root.systemLogoUrl
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    asynchronous: true
                    visible: root.systemLogoUrl !== ""
                    Layout.alignment: Qt.AlignHCenter
                }
                
                // Rating
                RowLayout {
                    spacing: 8
                    visible: root.gameRating > 0
                    
                    Label {
                        text: "Rating:"
                        font.pixelSize: 11
                        color: theme.textMuted
                    }
                    
                    Label {
                        text: root.gameRating.toFixed(1) + " / 10"
                        font.pixelSize: 12
                        font.bold: true
                        color: theme.accent
                    }
                    
                    Label {
                        text: "(" + root.ratingSource + ")"
                        font.pixelSize: 10
                        font.italic: true
                        color: theme.textMuted
                        visible: root.ratingSource !== ""
                    }
                }
                
                // Description
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(descriptionText.contentHeight + 20, 120)
                    Layout.maximumHeight: 120
                    color: theme.bgSecondary
                    radius: 4
                    visible: root.gameDescription !== ""
                    
                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 10
                        clip: true
                        
                        Label {
                            id: descriptionText
                            width: parent.width - 20
                            text: root.gameDescription
                            font.pixelSize: 11
                            color: theme.textSecondary
                            wrapMode: Text.WordWrap
                            textFormat: Text.PlainText
                        }
                    }
                }
                
                // Screenshots
                ColumnLayout {
                    spacing: 8
                    Layout.fillWidth: true
                    visible: root.titleScreenUrl !== "" || root.screenshotUrl !== ""
                    
                    Label {
                        text: "Screenshots"
                        font.bold: true
                        font.pixelSize: 12
                        color: theme.textPrimary
                    }
                    
                    // Title Screen
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 100
                        color: theme.bgSecondary
                        radius: 4
                        visible: root.titleScreenUrl !== ""
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 2
                            
                            Label {
                                text: "Start Screen"
                                font.pixelSize: 10
                                color: theme.textMuted
                                Layout.leftMargin: 6
                                Layout.topMargin: 4
                            }
                            
                            Image {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                source: root.titleScreenUrl
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                asynchronous: true
                            }
                        }
                    }
                    
                    // Gameplay Screenshot
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 100
                        color: theme.bgSecondary
                        radius: 4
                        visible: root.screenshotUrl !== ""
                        
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 2
                            
                            Label {
                                text: "Gameplay"
                                font.pixelSize: 10
                                color: theme.textMuted
                                Layout.leftMargin: 6
                                Layout.topMargin: 4
                            }
                            
                            Image {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                source: root.screenshotUrl
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                asynchronous: true
                            }
                        }
                    }
                }
            }
        }
        
        // === Hash Tools Section ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 0
            Layout.preferredHeight: hashContent.implicitHeight + 24
            color: theme.cardBg
            radius: 8
            
            ColumnLayout {
                id: hashContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                
                RowLayout {
                    spacing: 8
                    
                    Label {
                        text: "Hash & Verification"
                        font.bold: true
                        font.pixelSize: 13
                        color: theme.textPrimary
                        Layout.fillWidth: true
                    }
                    
                    Label {
                        text: "● Calculating..."
                        font.pixelSize: 10
                        font.italic: true
                        color: theme.warning
                        visible: !root.hasHashes
                    }
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }
                
                ThemedButton {
                    text: root.hasHashes ? "Recalculate Hash" : "Calculate Hash"
                    Layout.fillWidth: true
                    enabled: !root.hasHashes  // Disable during processing
                    onClicked: root.calculateHashRequested(root.fileId)
                }
                
                // Hash values - now with full display and live updates
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    spacing: 8
                    
                    // CRC32
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        
                        RowLayout {
                            spacing: 4
                            
                            Label {
                                text: "CRC32"
                                font.pixelSize: 11
                                font.bold: true
                                color: theme.textMuted
                            }
                            
                            Label {
                                text: "✓"
                                font.pixelSize: 10
                                color: theme.success
                                visible: root.crc32 !== ""
                                font.bold: true
                            }
                        }
                        
                        Rectangle {
                            Layout.fillWidth: true
                            height: 28
                            color: theme.inputBg
                            radius: 4
                            border.color: root.crc32 ? theme.success : theme.border
                            border.width: 1
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8
                                
                                Label {
                                    text: root.crc32 || "(not calculated)"
                                    font.pixelSize: 11
                                    font.family: root.crc32 ? "monospace" : Qt.application.font.family
                                    color: root.crc32 ? theme.textSecondary : theme.textMuted
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                
                                Label {
                                    text: "📋"
                                    font.pixelSize: 12
                                    visible: root.crc32 !== ""
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            // Copy to clipboard
                                            var clipboard = Qt.application.clipboard
                                            if (clipboard) {
                                                clipboard.setText(root.crc32)
                                            }
                                        }
                                        hoverEnabled: true
                                        onEntered: parent.opacity = 0.7
                                        onExited: parent.opacity = 1.0
                                    }
                                }
                            }
                        }
                    }
                    
                    // MD5
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        
                        RowLayout {
                            spacing: 4
                            
                            Label {
                                text: "MD5"
                                font.pixelSize: 11
                                font.bold: true
                                color: theme.textMuted
                            }
                            
                            Label {
                                text: "✓"
                                font.pixelSize: 10
                                color: theme.success
                                visible: root.md5 !== ""
                                font.bold: true
                            }
                        }
                        
                        Rectangle {
                            Layout.fillWidth: true
                            height: 28
                            color: theme.inputBg
                            radius: 4
                            border.color: root.md5 ? theme.success : theme.border
                            border.width: 1
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8
                                
                                Label {
                                    text: root.md5 || "(not calculated)"
                                    font.pixelSize: 11
                                    font.family: root.md5 ? "monospace" : Qt.application.font.family
                                    color: root.md5 ? theme.textSecondary : theme.textMuted
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                
                                Label {
                                    text: "📋"
                                    font.pixelSize: 12
                                    visible: root.md5 !== ""
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var clipboard = Qt.application.clipboard
                                            if (clipboard) {
                                                clipboard.setText(root.md5)
                                            }
                                        }
                                        hoverEnabled: true
                                        onEntered: parent.opacity = 0.7
                                        onExited: parent.opacity = 1.0
                                    }
                                }
                            }
                        }
                    }
                    
                    // SHA1
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        
                        RowLayout {
                            spacing: 4
                            
                            Label {
                                text: "SHA1"
                                font.pixelSize: 11
                                font.bold: true
                                color: theme.textMuted
                            }
                            
                            Label {
                                text: "✓"
                                font.pixelSize: 10
                                color: theme.success
                                visible: root.sha1 !== ""
                                font.bold: true
                            }
                        }
                        
                        Rectangle {
                            Layout.fillWidth: true
                            height: 28
                            color: theme.inputBg
                            radius: 4
                            border.color: root.sha1 ? theme.success : theme.border
                            border.width: 1
                            
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8
                                
                                Label {
                                    text: root.sha1 || "(not calculated)"
                                    font.pixelSize: 11
                                    font.family: root.sha1 ? "monospace" : Qt.application.font.family
                                    color: root.sha1 ? theme.textSecondary : theme.textMuted
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                
                                Label {
                                    text: "📋"
                                    font.pixelSize: 12
                                    visible: root.sha1 !== ""
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            var clipboard = Qt.application.clipboard
                                            if (clipboard) {
                                                clipboard.setText(root.sha1)
                                            }
                                        }
                                        hoverEnabled: true
                                        onEntered: parent.opacity = 0.7
                                        onExited: parent.opacity = 1.0
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // === Processing Options Section ===
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.topMargin: 0
            Layout.preferredHeight: optionsContent.implicitHeight + 24
            color: theme.cardBg
            radius: 8
            
            ColumnLayout {
                id: optionsContent
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                
                Label {
                    text: "Processing Options"
                    font.bold: true
                    font.pixelSize: 13
                    color: theme.textPrimary
                }
                
                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: theme.border
                }
                
                CheckBox {
                    id: fetchMetadataCheck
                    text: "Fetch Metadata"
                    checked: processingController.fetchMetadata
                    onCheckedChanged: processingController.fetchMetadata = checked
                    
                    contentItem: Label {
                        text: fetchMetadataCheck.text
                        font.pixelSize: 12
                        color: theme.textPrimary
                        leftPadding: fetchMetadataCheck.indicator.width + 8
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                CheckBox {
                    id: downloadArtworkCheck
                    text: "Download Artwork"
                    checked: processingController.downloadArtwork
                    onCheckedChanged: processingController.downloadArtwork = checked
                    enabled: processingController.fetchMetadata
                    
                    contentItem: Label {
                        text: downloadArtworkCheck.text
                        font.pixelSize: 12
                        color: downloadArtworkCheck.enabled ? theme.textPrimary : theme.textMuted
                        leftPadding: downloadArtworkCheck.indicator.width + 8
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                CheckBox {
                    id: convertToChdCheck
                    text: "Convert to CHD"
                    checked: processingController.convertToChd
                    onCheckedChanged: processingController.convertToChd = checked
                    
                    contentItem: Label {
                        text: convertToChdCheck.text
                        font.pixelSize: 12
                        color: theme.textPrimary
                        leftPadding: convertToChdCheck.indicator.width + 8
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                Label {
                    text: "(CHD for disc-based games only)"
                    font.pixelSize: 10
                    font.italic: true
                    color: theme.textMuted
                    Layout.leftMargin: 24
                    visible: convertToChdCheck.checked
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
                    text: "Preview Contents"
                    Layout.fillWidth: true
                    visible: root.isInsideArchive
                    onClicked: root.previewRequested(root.archivePath)
                }
                
                ThemedButton {
                    text: "Remove from Library"
                    Layout.fillWidth: true
                    onClicked: root.removeRequested(root.fileId)
                }
            }
        }
        
        // Spacer
        Item { Layout.fillHeight: true; Layout.minimumHeight: 20 }
    }
    
    function formatFileSize(bytes) {
        if (!bytes || bytes <= 0) return "0 B";
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB";
        if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + " MB";
        return (bytes / 1073741824).toFixed(2) + " GB";
    }
}
