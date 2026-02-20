import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import Remus 1.0
import "components"

/**
 * Simplified ROM Library View
 * Two-section layout: Unprocessed / Processed with contextual sidebar
 */
Page {
    id: libraryPage
    title: "Library"
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
    
    // Enable focus for keyboard shortcuts
    focus: true
    
    // Keyboard shortcuts
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Space && selectedFileId >= 0 && !selectedIsProcessed) {
            // Toggle selection of current file
            fileListModel.toggleSelected(selectedFileId);
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            // Start processing if files are selected
            if (fileListModel.selectedCount > 0 && !isProcessing) {
                startProcessing();
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_A && (event.modifiers & Qt.ControlModifier)) {
            // Ctrl+A to select all unprocessed
            fileListModel.selectAllUnprocessed(true);
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape) {
            // Escape to cancel processing or clear selection
            if (isProcessing) {
                processingController.cancelProcessing();
            } else {
                fileListModel.selectAllUnprocessed(false);
                selectedFileId = -1;
            }
            event.accepted = true;
        }
    }
    
    // Section visibility
    property bool unprocessedExpanded: true
    property bool processedExpanded: true
    property bool processingQueueExpanded: true
    
    // Currently selected file
    property int selectedFileId: -1
    property bool selectedIsProcessed: false
    
    // Processing state
    property bool isProcessing: false
    property string processingStatus: "Idle"
    property string currentProcessingFile: ""
    property real processingProgress: 0
    
    // Error state
    property string lastError: ""
    
    Timer {
        id: errorTimer
        interval: 5000
        onTriggered: lastError = ""
    }
    
    // Sync select all checkbox with model
    property bool selectAllChecked: fileListModel.selectedCount === fileListModel.unprocessedCount && fileListModel.unprocessedCount > 0
    
    // Refresh signals
    Connections {
        target: fileListModel
        function onCountChanged() { }
        function onSelectionChanged() { selectAllChecked = fileListModel.selectedCount === fileListModel.unprocessedCount && fileListModel.unprocessedCount > 0 }
    }
    
    Connections {
        target: matchController
        function onLibraryUpdated() { fileListModel.refresh(); }
    }
    
    Connections {
        target: libraryController
        function onLibraryUpdated() { fileListModel.refresh(); }
    }
    
    Connections {
        target: conversionController
        function onConversionCompleted() { fileListModel.refresh(); }
    }
    
    // ProcessingController connections
    Connections {
        target: processingController
        
        function onProcessingChanged() {
            isProcessing = processingController.processing;
        }
        
        function onProgressChanged() {
            processingProgress = processingController.overallProgress;
        }
        
        function onCurrentFileChanged() {
            currentProcessingFile = processingController.currentFilename;
        }
        
        function onStatusMessageChanged() {
            processingStatus = processingController.statusMessage;
        }
        
        function onProcessingCompleted(successCount, failCount) {
            isProcessing = false;
            processingStatus = "Complete: " + successCount + " processed" + 
                              (failCount > 0 ? ", " + failCount + " failed" : "");
            fileListModel.refresh();
        }
        
        function onProcessingCancelled() {
            isProcessing = false;
            processingStatus = "Cancelled";
        }
        
        function onProcessingError(fileId, step, error) {
            lastError = error;
            errorTimer.restart();
        }
        
        function onLibraryUpdated() {
            fileListModel.refresh();
        }
    }
    
    // Header bar
    header: ToolBar {
        background: Rectangle { color: theme.cardBg }
        height: 56
        
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12
            
            Label {
                text: "ROM Library"
                font.pixelSize: 20
                font.bold: true
                color: theme.textPrimary
            }
            
            Item { Layout.fillWidth: true }
            
            ThemedButton {
                text: "Refresh"
                icon.name: "view-refresh"
                onClicked: libraryController.refreshList()
            }
            
            ThemedButton {
                text: "Scan Directory"
                icon.name: "folder-open"
                onClicked: folderDialog.open()
            }
            
            ThemedButton {
                text: "Begin"
                icon.name: "media-playback-start"
                isPrimary: true
                enabled: fileListModel.selectedCount > 0 && !isProcessing
                onClicked: startProcessing()
            }
        }
    }
    
    // Drag-drop import support
    property bool dropActive: false
    
    DropArea {
        id: dropArea
        anchors.fill: parent
        enabled: !isProcessing
        
        onEntered: (drag) => {
            if (drag.hasUrls) {
                dropActive = true;
                drag.accepted = true;
            } else {
                drag.accepted = false;
            }
        }
        
        onExited: {
            dropActive = false;
        }
        
        onDropped: (drop) => {
            dropActive = false;
            if (drop.hasUrls && drop.urls.length > 0) {
                // Get the first dropped folder/file
                var firstUrl = drop.urls[0].toString();
                // Remove file:// prefix
                var path = firstUrl.replace("file://", "");
                // Decode URI components
                path = decodeURIComponent(path);
                
                // Scan the dropped directory
                libraryController.scanDirectory(path);
            }
        }
    }
    
    // Drop overlay
    Rectangle {
        anchors.fill: parent
        color: theme.primary
        opacity: dropActive ? 0.2 : 0
        z: 100
        visible: opacity > 0
        
        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }
        
        Rectangle {
            anchors.centerIn: parent
            width: dropLabel.width + 48
            height: dropLabel.height + 32
            radius: 12
            color: theme.primary
            opacity: parent.opacity > 0 ? 1 : 0
            
            Label {
                id: dropLabel
                anchors.centerIn: parent
                text: "Drop folder to import ROMs"
                font.pixelSize: 18
                font.bold: true
                color: "white"
            }
        }
    }
    
    // Main content
    RowLayout {
        anchors.fill: parent
        spacing: 0
        
        // Left side: File lists
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 400
            color: theme.mainBg
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                
                // Scrollable list area
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    
                    ColumnLayout {
                        width: parent.width
                        spacing: 0
                        
                        // === UNPROCESSED SECTION ===
                        SectionHeader {
                            Layout.fillWidth: true
                            title: "Unprocessed"
                            count: fileListModel.unprocessedCount
                            expanded: unprocessedExpanded
                            showSelectAll: true
                            selectAllChecked: libraryPage.selectAllChecked
                            onToggleExpanded: unprocessedExpanded = !unprocessedExpanded
                            onSelectAllToggled: (checked) => {
                                fileListModel.selectAllUnprocessed(checked)
                            }
                        }
                        
                        // Unprocessed list
                        Repeater {
                            model: unprocessedExpanded ? fileListModel : null
                            
                            delegate: Loader {
                                active: !model.isProcessed
                                visible: active
                                width: parent.width
                                height: active ? 48 : 0
                                
                                sourceComponent: UnprocessedListItem {
                                    fileId: model.fileId
                                    filename: model.filename
                                    extensions: model.extensions || ""
                                    archiveExtension: model.archiveExtension || ""
                                    isSelected: model.isSelected
                                    isHighlighted: selectedFileId === model.fileId
                                    
                                    onSelectionToggled: (fid, selected) => {
                                        fileListModel.setSelected(fid, selected)
                                    }
                                    onItemClicked: (fid) => {
                                        selectedFileId = fid
                                        selectedIsProcessed = false
                                        updateDetailPanel(fid, false)
                                    }
                                }
                            }
                        }
                        
                        // Empty state for unprocessed
                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 20
                            Layout.bottomMargin: 20
                            text: "No unprocessed ROMs"
                            horizontalAlignment: Text.AlignHCenter
                            color: theme.textMuted
                            visible: unprocessedExpanded && fileListModel.unprocessedCount === 0
                        }
                        
                        // === PROCESSING QUEUE SECTION (when active) ===
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            Layout.preferredHeight: processingQueueExpanded ? 180 : 40
                            color: theme.cardBg
                            radius: 4
                            visible: isProcessing
                            border.color: theme.info
                            border.width: 2
                            
                            Behavior on Layout.preferredHeight {
                                NumberAnimation { duration: 200 }
                            }
                            
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8
                                
                                // Header row
                                RowLayout {
                                    spacing: 8
                                    
                                    Text {
                                        text: "⚙"
                                        font.pixelSize: 16
                                        color: theme.info
                                    }
                                    
                                    Label {
                                        text: "Processing Queue (" + processingController.currentFileIndex + 1 + " of " + processingController.totalFiles + ")"
                                        font.bold: true
                                        font.pixelSize: 12
                                        color: theme.textPrimary
                                        Layout.fillWidth: true
                                    }
                                    
                                    MouseArea {
                                        width: 20
                                        height: 20
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: processingQueueExpanded = !processingQueueExpanded
                                        
                                        Text {
                                            anchors.centerIn: parent
                                            text: processingQueueExpanded ? "▼" : "▶"
                                            font.pixelSize: 12
                                            color: theme.textMuted
                                        }
                                    }
                                }
                                
                                // Queue details (when expanded)
                                ColumnLayout {
                                    visible: processingQueueExpanded
                                    Layout.fillWidth: true
                                    spacing: 4
                                    
                                    // Current file (highlighted)
                                    Rectangle {
                                        Layout.fillWidth: true
                                        height: 32
                                        color: theme.primary
                                        radius: 3
                                        opacity: 0.2
                                        
                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8
                                            anchors.rightMargin: 8
                                            spacing: 8
                                            
                                            Text {
                                                text: "▶"
                                                font.pixelSize: 10
                                                color: theme.primary
                                                font.bold: true
                                            }
                                            
                                            ColumnLayout {
                                                spacing: 2
                                                Layout.fillWidth: true
                                                
                                                Label {
                                                    text: currentProcessingFile
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                    color: theme.textPrimary
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                                
                                                Label {
                                                    text: processingController.currentStep
                                                    font.pixelSize: 9
                                                    color: theme.textMuted
                                                }
                                            }
                                        }
                                    }
                                    
                                    // Remaining files count
                                    Label {
                                        text: (processingController.totalFiles - processingController.currentFileIndex - 1) + " files remaining"
                                        font.pixelSize: 9
                                        color: theme.textMuted
                                        visible: processingController.totalFiles - processingController.currentFileIndex - 1 > 0
                                    }
                                }
                            }
                        }
                        
                        // === PROCESSED SECTION ===
                        SectionHeader {
                            Layout.fillWidth: true
                            Layout.topMargin: 8
                            title: "Processed"
                            count: fileListModel.processedCount
                            expanded: processedExpanded
                            showSelectAll: false
                            onToggleExpanded: processedExpanded = !processedExpanded
                        }
                        
                        // Processed list
                        Repeater {
                            model: processedExpanded ? fileListModel : null
                            
                            delegate: Loader {
                                active: model.isProcessed
                                visible: active
                                width: parent.width
                                height: active ? 56 : 0
                                
                                sourceComponent: ProcessedListItem {
                                    fileId: model.fileId
                                    title: model.matchedTitle || model.filename
                                    system: model.systemName
                                    year: model.matchYear || 0
                                    matchConfidence: model.matchConfidence || 0
                                    isHighlighted: selectedFileId === model.fileId
                                    
                                    onItemClicked: (fid) => {
                                        selectedFileId = fid
                                        selectedIsProcessed = true
                                        updateDetailPanel(fid, true)
                                    }
                                }
                            }
                        }
                        
                        // Empty state for processed
                        Label {
                            Layout.fillWidth: true
                            Layout.topMargin: 20
                            Layout.bottomMargin: 20
                            text: "No processed ROMs yet\n\nSelect ROMs above and click 'Begin'"
                            horizontalAlignment: Text.AlignHCenter
                            color: theme.textMuted
                            visible: processedExpanded && fileListModel.processedCount === 0
                        }
                        
                        // Spacer at bottom
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 20
                        }
                    }
                }
                
                // Progress bar at bottom
                Rectangle {
                    Layout.fillWidth: true
                    height: 48
                    color: theme.cardBg
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12
                        
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            
                            Label {
                                text: isProcessing ? 
                                    (processingController.currentFileIndex + 1) + " of " + processingController.totalFiles + ": " + currentProcessingFile :
                                    processingStatus
                                color: theme.textSecondary
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                                font.pointSize: 10
                            }
                            
                            ThemedProgressBar {
                                visible: isProcessing
                                value: processingProgress
                                Layout.fillWidth: true
                                height: 6
                            }
                        }
                        
                        Label {
                            visible: isProcessing
                            text: Math.round(processingProgress * 100) + "%"
                            color: theme.textSecondary
                            font.bold: true
                            font.pointSize: 10
                        }
                    }
                    
                    // Top border
                    Rectangle {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: theme.border
                    }
                }
            }
        }
        
        // Vertical separator
        Rectangle {
            Layout.fillHeight: true
            width: 1
            color: theme.border
        }
        
        // Right side: Contextual sidebar
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 320
            Layout.minimumWidth: 280
            color: theme.mainBg
            
            // Sidebar content based on selection
            Item {
                anchors.fill: parent
                
                // Empty state (no selection)
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    visible: selectedFileId < 0
                    
                    Item { Layout.fillHeight: true }
                    
                    Label {
                        text: "Select a ROM"
                        font.pixelSize: 16
                        color: theme.textMuted
                        Layout.alignment: Qt.AlignHCenter
                    }
                    
                    Label {
                        text: "Click on a ROM to see details"
                        font.pixelSize: 12
                        color: theme.textMuted
                        Layout.alignment: Qt.AlignHCenter
                    }
                    
                    Item { Layout.fillHeight: true }
                }
                
                // Unprocessed sidebar
                UnprocessedSidebar {
                    anchors.fill: parent
                    visible: selectedFileId >= 0 && !selectedIsProcessed
                    
                    fileId: selectedFileId
                    filename: sidebarData.filename
                    filePath: sidebarData.filePath
                    extensions: sidebarData.extensions
                    fileSize: sidebarData.fileSize
                    systemId: sidebarData.systemId
                    systemName: sidebarData.systemName
                    isInsideArchive: sidebarData.isInsideArchive
                    possibleMatches: sidebarData.possibleMatches
                    crc32: sidebarData.hashCRC32
                    md5: sidebarData.hashMD5
                    sha1: sidebarData.hashSHA1
                    
                    onCalculateHashRequested: (fid) => {
                        libraryController.hashFile(fid)
                    }
                    onRemoveRequested: (fid) => {
                        libraryController.removeFile(fid)
                        fileListModel.refresh()
                        selectedFileId = -1
                    }
                    onMatchSelected: (matchData) => {
                        // Trigger a fresh match attempt for the file.
                        // In a future iteration this should apply matchData.gameId directly.
                        matchController.matchFile(fid)
                        fileListModel.refresh()
                    }
                }
                
                // Processed sidebar
                ProcessedSidebar {
                    anchors.fill: parent
                    visible: selectedFileId >= 0 && selectedIsProcessed
                    
                    fileId: selectedFileId
                    filename: sidebarData.filename
                    filePath: sidebarData.filePath
                    fileSize: sidebarData.fileSize
                    systemId: sidebarData.systemId
                    systemName: sidebarData.systemName
                    matchedTitle: sidebarData.matchedTitle
                    matchYear: sidebarData.matchYear
                    matchPublisher: sidebarData.matchPublisher
                    matchDeveloper: sidebarData.matchDeveloper
                    matchConfidence: sidebarData.matchConfidence
                    matchMethod: sidebarData.matchMethod
                    description: sidebarData.matchDescription
                    genre: sidebarData.matchGenre
                    region: sidebarData.matchRegion
                    rating: sidebarData.matchRating
                    coverArtPath: sidebarData.coverArtPath
                    screenshotPaths: sidebarData.screenshotPaths
                    crc32: sidebarData.hashCRC32
                    
                    onEditMetadataRequested: (fid) => {
                        const matchInfo = metadataEditor.getMatchInfo(fid)
                        if (matchInfo && matchInfo.gameId > 0) {
                            StackView.view.push(Qt.resolvedUrl("GameDetailsView.qml"),
                                                { gameId: matchInfo.gameId })
                        }
                    }
                    onRematchRequested: (fid) => {
                        matchController.matchFile(fid)
                        fileListModel.refresh()
                    }
                    onOpenFolderRequested: (path) => {
                        console.log("Open folder:", path)
                        Qt.openUrlExternally("file://" + path)
                    }
                    onScreenshotClicked: (path) => {
                        Qt.openUrlExternally("file://" + path)
                    }
                }
            }
        }
    }
    
    // Sidebar data holder
    QtObject {
        id: sidebarData
        property string filename: ""
        property string filePath: ""
        property string extensions: ""
        property int fileSize: 0
        property int systemId: 0
        property string systemName: ""  // System display name from model
        property bool isInsideArchive: false
        property string matchedTitle: ""
        property string matchPublisher: ""
        property string matchDeveloper: ""
        property string matchGenre: ""
        property string matchRegion: ""
        property string matchDescription: ""
        property int matchYear: 0
        property int matchConfidence: 0
        property string matchMethod: ""
        property real matchRating: 0
        property string coverArtPath: ""
        property var screenshotPaths: []
        property var possibleMatches: []
        property string hashCRC32: ""
        property string hashMD5: ""
        property string hashSHA1: ""
    }
    
    // Update detail panel when selection changes
    function updateDetailPanel(fileId, isProcessed) {
        for (var i = 0; i < fileListModel.count; i++) {
            var idx = fileListModel.index(i, 0);
            var fid = fileListModel.data(idx, FileListModel.IdRole);
            if (fid === fileId) {
                sidebarData.filename = fileListModel.data(idx, FileListModel.FilenameRole) || "";
                sidebarData.filePath = fileListModel.data(idx, FileListModel.PathRole) || "";
                sidebarData.extensions = fileListModel.data(idx, FileListModel.ExtensionsRole) || "";
                sidebarData.fileSize = fileListModel.data(idx, FileListModel.FileSizeRole) || 0;
                sidebarData.systemId = fileListModel.data(idx, FileListModel.SystemRole) || 0;
                sidebarData.systemName = fileListModel.data(idx, FileListModel.SystemNameRole) || "Unknown";
                sidebarData.isInsideArchive = fileListModel.data(idx, FileListModel.IsInsideArchiveRole) || false;
                sidebarData.matchedTitle = fileListModel.data(idx, FileListModel.MatchedTitleRole) || "";
                sidebarData.matchPublisher = fileListModel.data(idx, FileListModel.MatchPublisherRole) || "";
                sidebarData.matchYear = fileListModel.data(idx, FileListModel.MatchYearRole) || 0;
                sidebarData.matchConfidence = fileListModel.data(idx, FileListModel.MatchConfidenceRole) || 0;
                sidebarData.matchMethod = fileListModel.data(idx, FileListModel.MatchMethodRole) || "";
                // New properties - try to get from model if available
                sidebarData.matchDeveloper = fileListModel.data(idx, FileListModel.MatchDeveloperRole) || "";
                sidebarData.matchGenre = fileListModel.data(idx, FileListModel.MatchGenreRole) || "";
                sidebarData.matchRegion = fileListModel.data(idx, FileListModel.MatchRegionRole) || "";
                sidebarData.matchDescription = fileListModel.data(idx, FileListModel.MatchDescriptionRole) || "";
                sidebarData.matchRating = fileListModel.data(idx, FileListModel.MatchRatingRole) ||0;
                sidebarData.coverArtPath = artworkController.getArtworkPath(
                    fileListModel.data(idx, FileListModel.IdRole) || 0, "boxart");
                sidebarData.hashCRC32 = fileListModel.data(idx, FileListModel.Crc32Role) || "";
                sidebarData.hashMD5 = fileListModel.data(idx, FileListModel.Md5Role) || "";
                sidebarData.hashSHA1 = fileListModel.data(idx, FileListModel.Sha1Role) || "";
                // These would come from a match lookup - placeholder for now
                sidebarData.screenshotPaths = [];
                sidebarData.possibleMatches = [];
                break;
            }
        }
    }
    
    // Start processing selected ROMs
    function startProcessing() {
        var selected = fileListModel.getSelectedUnprocessed();
        if (selected.length === 0) return;
        
        // Build list of file IDs
        var fileIds = [];
        for (var i = 0; i < selected.length; i++) {
            fileIds.push(selected[i].fileId);
        }
        
        console.log("Starting processing for", fileIds.length, "ROMs:", fileIds);
        
        // Start the processing pipeline
        processingController.startProcessing(fileIds);
    }
    
    // NOTE: System names are provided by FileListModel.systemName (SystemNameRole)
    // DO NOT create hardcoded system ID maps - they will become incorrect when
    // the database schema or constants change. Use the model's data directly.
    
    function formatFileSize(bytes) {
        if (!bytes || bytes <= 0) return "0 B";
        if (bytes < 1024) return bytes + " B";
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB";
        if (bytes < 1073741824) return (bytes / 1048576).toFixed(1) + " MB";
        return (bytes / 1073741824).toFixed(2) + " GB";
    }
    
    FolderDialog {
        id: folderDialog
        title: "Select ROM Directory"
        onAccepted: {
            libraryController.scanDirectory(selectedFolder.toString().replace("file://", ""))
        }
    }
    
    // Error toast notification
    Rectangle {
        id: errorToast
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 60
        anchors.horizontalCenter: parent.horizontalCenter
        width: errorLabel.width + 32
        height: errorLabel.height + 20
        radius: 8
        color: theme.danger
        opacity: lastError !== "" ? 1 : 0
        visible: opacity > 0
        z: 1000
        
        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
        
        Label {
            id: errorLabel
            anchors.centerIn: parent
            text: lastError
            color: "white"
            font.pixelSize: 13
        }
        
        MouseArea {
            anchors.fill: parent
            onClicked: lastError = ""
        }
    }
}
