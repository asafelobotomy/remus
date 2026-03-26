#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QSet>
#include <QStringList>
#include "../../core/database.h"
#include "../../core/system_resolver.h"

namespace Remus {

/**
 * @brief Workflow states for the unified ROM processing pipeline
 */
enum class WorkflowState {
    NotApplicable = 0,   ///< State not relevant for this file type
    NeedsAction,         ///< Requires user/auto action
    InProgress,          ///< Currently being processed
    Complete,            ///< Successfully completed
    Failed,              ///< Action failed
    Skipped              ///< User skipped this step
};

/**
 * @brief Match info from the matches table
 */
struct MatchInfo {
    int gameId = 0;
    int confidence = 0;           ///< 0-100
    QString matchMethod;          ///< "hash", "filename", "fuzzy", "manual"
    QString matchedTitle;         ///< Game title from games table
    QString publisher;            ///< Publisher name
    QString developer;            ///< Developer name
    int year = 0;                 ///< Release year
    QString description;          ///< Game description
    QString genre;                ///< Genres (comma separated)
    QString players;              ///< Player count
    QString region;               ///< Region
    float rating = 0.0f;          ///< Rating (0-10)
    bool isConfirmed = false;     ///< User confirmed
    bool isRejected = false;      ///< User rejected
};

/**
 * @brief Represents a grouped entry (game) that may contain multiple files
 */
struct FileGroupEntry {
    // === Basic file info ===
    int primaryFileId = 0;           ///< ID of the primary file (.cue, .gdi, etc.)
    QString displayName;              ///< Name to show (without extension)
    QString currentPath;              ///< Path to primary file
    QStringList extensions;           ///< All extensions in this group [".cue", ".bin"]
    qint64 totalSize = 0;            ///< Total size of all files
    int systemId = 0;                ///< System ID
    QString systemName;              ///< System display name
    int fileCount = 1;               ///< Number of files in this group
    QString lastModified;            ///< Last modified date
    QList<int> allFileIds;           ///< All file IDs in this group
    
    // === Processed status ===
    bool isProcessed = false;        ///< Has been fully processed
    QString processingStatus;        ///< Status: unprocessed, processing, processed, failed
    
    // === Workflow states ===
    WorkflowState extractionState = WorkflowState::NotApplicable;
    WorkflowState chdState = WorkflowState::NotApplicable;
    WorkflowState hashState = WorkflowState::NeedsAction;
    WorkflowState matchState = WorkflowState::NeedsAction;
    
    // === Source info ===
    bool isInsideArchive = false;    ///< File is still inside a compressed archive
    QString archivePath;              ///< Path to source archive if applicable
    QString archiveExtension;         ///< Archive extension (.zip, .7z, .rar)
    bool isChdCandidate = false;     ///< System supports CHD compression
    bool isAlreadyChd = false;       ///< Already in CHD format
    
    // === Hash info ===
    bool hasHashes = false;          ///< Has at least one hash calculated
    QString crc32;
    QString md5;
    QString sha1;
    
    // === Match info ===
    MatchInfo matchInfo;             ///< Match details if matched
};

/**
 * @brief Qt model exposing file library to QML
 * 
 * Provides a list view of scanned ROM files with system grouping,
 * filtering, and sorting capabilities. Multi-file games (e.g., .cue + .bin)
 * are grouped and displayed as a single entry.
 */
class FileListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int unprocessedCount READ unprocessedCount NOTIFY countsChanged)
    Q_PROPERTY(int processedCount READ processedCount NOTIFY countsChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QString systemFilter READ systemFilter WRITE setSystemFilter NOTIFY systemFilterChanged)
    Q_PROPERTY(bool showMatchedOnly READ showMatchedOnly WRITE setShowMatchedOnly NOTIFY showMatchedOnlyChanged)
    
public:
    enum FileRole {
        IdRole = Qt::UserRole + 1,
        FilenameRole,
        PathRole,
        ExtensionRole,       ///< Primary extension
        ExtensionsRole,      ///< All extensions as comma-separated string
        FileSizeRole,
        SystemRole,          ///< System ID (int)
        SystemNameRole,      ///< System display name (QString)
        MatchedRole,
        IsPrimaryRole,
        LastModifiedRole,
        Crc32Role,
        Md5Role,
        Sha1Role,
        FileCountRole,       ///< Number of files in this group
        AllFileIdsRole,      ///< All file IDs as comma-separated string
        
        // Workflow state roles
        ExtractionStateRole, ///< WorkflowState for extraction
        ChdStateRole,        ///< WorkflowState for CHD conversion
        HashStateRole,       ///< WorkflowState for hashing
        MatchStateRole,      ///< WorkflowState for matching
        
        // Source info roles
        IsInsideArchiveRole, ///< True if still in archive
        ArchivePathRole,     ///< Path to source archive
        ArchiveExtensionRole, ///< Archive extension (.zip, .7z, etc)
        IsChdCandidateRole,  ///< True if CHD conversion available
        IsAlreadyChdRole,    ///< True if already CHD
        
        // Match info roles
        MatchConfidenceRole, ///< 0-100 confidence
        MatchMethodRole,     ///< "hash", "filename", "fuzzy", "manual"
        MatchedTitleRole,    ///< Matched game title
        MatchPublisherRole,  ///< Publisher name
        MatchDeveloperRole,  ///< Developer name
        MatchYearRole,       ///< Release year
        MatchGenreRole,      ///< Genre(s)
        MatchRegionRole,     ///< Region
        MatchDescriptionRole,///< Description
        MatchRatingRole,     ///< Rating 0-10
        MatchPlayersRole,    ///< Player count
        MatchConfirmedRole,  ///< User confirmed
        MatchRejectedRole,   ///< User rejected
        
        // Processed state roles
        IsProcessedRole,     ///< True if fully processed
        IsSelectedRole,      ///< True if selected (checkbox)
        ProcessingStatusRole, ///< Status string
        
        // Pipeline summary
        PipelineProgressRole ///< 0-100 overall progress
    };
    Q_ENUM(FileRole)
    
    explicit FileListModel(Database *db = nullptr, QObject *parent = nullptr);
    
    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    // Property getters/setters
    QString systemFilter() const { return m_systemFilter; }
    void setSystemFilter(const QString &filter);
    
    bool showMatchedOnly() const { return m_showMatchedOnly; }
    void setShowMatchedOnly(bool value);
    
    // Count getters
    int unprocessedCount() const { return m_unprocessedCount; }
    int processedCount() const { return m_processedCount; }
    int selectedCount() const { return m_selectedIds.size(); }
    
    // Database operations
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clear();
    
    // Selection operations
    Q_INVOKABLE void setSelected(int fileId, bool selected);
    Q_INVOKABLE void toggleSelected(int fileId);
    Q_INVOKABLE void selectAllUnprocessed(bool selected);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QVariantList getSelectedUnprocessed() const;
    Q_INVOKABLE bool isSelected(int fileId) const;
    
    void setDatabase(Database *db);
    
signals:
    void countChanged();
    void countsChanged();
    void selectionChanged();
    void systemFilterChanged();
    void showMatchedOnlyChanged();
    void errorOccurred(const QString &error);
    
private:
    void loadFiles();
    void groupFiles(const QList<FileRecord> &files, const QMap<int, Database::MatchResult> &matches);
    void updateCounts();
    QString extractBaseName(const QString &filename) const;
    
    Database *m_db = nullptr;
    QList<FileRecord> m_files;           // Raw file records
    QList<FileGroupEntry> m_groupedFiles; // Grouped entries for display
    QString m_systemFilter;
    bool m_showMatchedOnly = false;
    
    // Selection tracking
    QSet<int> m_selectedIds;             // File IDs currently selected
    int m_unprocessedCount = 0;          // Count of unprocessed files
    int m_processedCount = 0;            // Count of processed files
};

} // namespace Remus
