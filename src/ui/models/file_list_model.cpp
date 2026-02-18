#include "file_list_model.h"
#include <QDebug>
#include <QFileInfo>
#include <QFile>
#include <QMap>
#include <QDir>
#include <QRegularExpression>
#include "../../core/constants/systems.h"

namespace Remus {

using namespace Remus::Constants::Systems;

// Archive file extensions
static const QStringList ARCHIVE_EXTENSIONS = {
    ".zip", ".7z", ".rar", ".gz", ".tar", ".bz2", ".xz"
};

// CHD-compatible extensions (disc images)
static const QStringList CHD_SOURCE_EXTENSIONS = {
    ".cue", ".gdi", ".iso", ".bin", ".img", ".mdf", ".cdi", ".nrg"
};

FileListModel::FileListModel(Database *db, QObject *parent)
    : QAbstractListModel(parent)
    , m_db(db)
{
    if (m_db) {
        loadFiles();
    }
}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_groupedFiles.count();
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_groupedFiles.count())
        return QVariant();
    
    const FileGroupEntry &entry = m_groupedFiles.at(index.row());
    
    switch (role) {
    case IdRole:
        return entry.primaryFileId;
    case FilenameRole:
        return entry.displayName;
    case PathRole:
        return entry.currentPath;
    case ExtensionRole:
        return entry.extensions.isEmpty() ? QString() : entry.extensions.first();
    case ExtensionsRole:
        return entry.extensions.join("/");
    case FileSizeRole:
        return entry.totalSize;
    case SystemRole:
        return entry.systemId;
    case SystemNameRole:
        return entry.systemName;
    case MatchedRole:
        return entry.matchState == WorkflowState::Complete;
    case IsPrimaryRole:
        return true;  // All grouped entries are "primary" for display
    case LastModifiedRole:
        return entry.lastModified;
    case FileCountRole:
        return entry.fileCount;
    case AllFileIdsRole: {
        QStringList ids;
        for (int id : entry.allFileIds) {
            ids << QString::number(id);
        }
        return ids.join(",");
    }
    case Crc32Role:
        return entry.crc32;
    case Md5Role:
        return entry.md5;
    case Sha1Role:
        return entry.sha1;
        
    // Workflow state roles - convert enum to int for QML
    case ExtractionStateRole:
        return static_cast<int>(entry.extractionState);
    case ChdStateRole:
        return static_cast<int>(entry.chdState);
    case HashStateRole:
        return static_cast<int>(entry.hashState);
    case MatchStateRole:
        return static_cast<int>(entry.matchState);
        
    // Source info roles
    case IsInsideArchiveRole:
        return entry.isInsideArchive;
    case ArchivePathRole:
        return entry.archivePath;
    case ArchiveExtensionRole:
        return entry.archiveExtension;
    case IsChdCandidateRole:
        return entry.isChdCandidate;
    case IsAlreadyChdRole:
        return entry.isAlreadyChd;
        
    // Match info roles
    case MatchConfidenceRole:
        return entry.matchInfo.confidence;
    case MatchMethodRole:
        return entry.matchInfo.matchMethod;
    case MatchedTitleRole:
        return entry.matchInfo.matchedTitle;
    case MatchPublisherRole:
        return entry.matchInfo.publisher;
    case MatchDeveloperRole:
        return entry.matchInfo.developer;
    case MatchYearRole:
        return entry.matchInfo.year;
    case MatchGenreRole:
        return entry.matchInfo.genre;
    case MatchRegionRole:
        return entry.matchInfo.region;
    case MatchDescriptionRole:
        return entry.matchInfo.description;
    case MatchRatingRole:
        return entry.matchInfo.rating;
    case MatchPlayersRole:
        return entry.matchInfo.players;
    case MatchConfirmedRole:
        return entry.matchInfo.isConfirmed;
    case MatchRejectedRole:
        return entry.matchInfo.isRejected;
        
    // Processed state roles
    case IsProcessedRole:
        return entry.isProcessed;
    case IsSelectedRole:
        return m_selectedIds.contains(entry.primaryFileId);
    case ProcessingStatusRole:
        return entry.processingStatus;
        
    // Pipeline progress (count completed states / total applicable states * 100)
    case PipelineProgressRole: {
        int completed = 0;
        int total = 0;
        
        // Extraction
        if (entry.extractionState != WorkflowState::NotApplicable) {
            total++;
            if (entry.extractionState == WorkflowState::Complete) completed++;
        }
        // CHD
        if (entry.chdState != WorkflowState::NotApplicable) {
            total++;
            if (entry.chdState == WorkflowState::Complete) completed++;
        }
        // Hash
        if (entry.hashState != WorkflowState::NotApplicable) {
            total++;
            if (entry.hashState == WorkflowState::Complete) completed++;
        }
        // Match
        if (entry.matchState != WorkflowState::NotApplicable) {
            total++;
            if (entry.matchState == WorkflowState::Complete) completed++;
        }
        
        return total > 0 ? (completed * 100 / total) : 100;
    }
        
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "fileId";
    roles[FilenameRole] = "filename";
    roles[PathRole] = "path";
    roles[ExtensionRole] = "extension";
    roles[ExtensionsRole] = "extensions";
    roles[FileSizeRole] = "fileSize";
    roles[SystemRole] = "systemId";
    roles[SystemNameRole] = "systemName";
    roles[MatchedRole] = "matched";
    roles[IsPrimaryRole] = "isPrimary";
    roles[LastModifiedRole] = "lastModified";
    roles[Crc32Role] = "crc32";
    roles[Md5Role] = "md5";
    roles[Sha1Role] = "sha1";
    roles[FileCountRole] = "fileCount";
    roles[AllFileIdsRole] = "allFileIds";
    
    // Workflow state roles
    roles[ExtractionStateRole] = "extractionState";
    roles[ChdStateRole] = "chdState";
    roles[HashStateRole] = "hashState";
    roles[MatchStateRole] = "matchState";
    
    // Source info roles
    roles[IsInsideArchiveRole] = "isInsideArchive";
    roles[ArchivePathRole] = "archivePath";
    roles[ArchiveExtensionRole] = "archiveExtension";
    roles[IsChdCandidateRole] = "isChdCandidate";
    roles[IsAlreadyChdRole] = "isAlreadyChd";
    
    // Match info roles
    roles[MatchConfidenceRole] = "matchConfidence";
    roles[MatchMethodRole] = "matchMethod";
    roles[MatchedTitleRole] = "matchedTitle";
    roles[MatchPublisherRole] = "matchPublisher";
    roles[MatchDeveloperRole] = "matchDeveloper";
    roles[MatchYearRole] = "matchYear";
    roles[MatchGenreRole] = "matchGenre";
    roles[MatchRegionRole] = "matchRegion";
    roles[MatchDescriptionRole] = "matchDescription";
    roles[MatchRatingRole] = "matchRating";
    roles[MatchPlayersRole] = "matchPlayers";
    roles[MatchConfirmedRole] = "matchConfirmed";
    roles[MatchRejectedRole] = "matchRejected";
    
    // Processed state roles
    roles[IsProcessedRole] = "isProcessed";
    roles[IsSelectedRole] = "isSelected";
    roles[ProcessingStatusRole] = "processingStatus";
    
    // Pipeline
    roles[PipelineProgressRole] = "pipelineProgress";
    
    return roles;
}

void FileListModel::setSystemFilter(const QString &filter)
{
    if (m_systemFilter == filter)
        return;
    
    m_systemFilter = filter;
    emit systemFilterChanged();
    loadFiles();
}

void FileListModel::setShowMatchedOnly(bool value)
{
    if (m_showMatchedOnly == value)
        return;
    
    m_showMatchedOnly = value;
    emit showMatchedOnlyChanged();
    loadFiles();
}

void FileListModel::refresh()
{
    loadFiles();
}

void FileListModel::clear()
{
    beginResetModel();
    m_files.clear();
    m_groupedFiles.clear();
    endResetModel();
    emit countChanged();
}

void FileListModel::setDatabase(Database *db)
{
    m_db = db;
    if (m_db) {
        loadFiles();
    }
}

void FileListModel::setSelected(int fileId, bool selected)
{
    if (selected) {
        m_selectedIds.insert(fileId);
    } else {
        m_selectedIds.remove(fileId);
    }
    
    // Find the row that changed and emit dataChanged
    for (int i = 0; i < m_groupedFiles.count(); ++i) {
        if (m_groupedFiles[i].primaryFileId == fileId) {
            QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {IsSelectedRole});
            break;
        }
    }
    
    emit selectionChanged();
}

void FileListModel::toggleSelected(int fileId)
{
    bool currentlySelected = m_selectedIds.contains(fileId);
    setSelected(fileId, !currentlySelected);
}

void FileListModel::selectAllUnprocessed(bool selected)
{
    for (const FileGroupEntry &entry : m_groupedFiles) {
        if (!entry.isProcessed) {
            if (selected) {
                m_selectedIds.insert(entry.primaryFileId);
            } else {
                m_selectedIds.remove(entry.primaryFileId);
            }
        }
    }
    
    // Emit dataChanged for all rows
    if (!m_groupedFiles.isEmpty()) {
        emit dataChanged(index(0), index(m_groupedFiles.count() - 1), {IsSelectedRole});
    }
    
    emit selectionChanged();
}

void FileListModel::clearSelection()
{
    m_selectedIds.clear();
    
    if (!m_groupedFiles.isEmpty()) {
        emit dataChanged(index(0), index(m_groupedFiles.count() - 1), {IsSelectedRole});
    }
    
    emit selectionChanged();
}

QVariantList FileListModel::getSelectedUnprocessed() const
{
    QVariantList result;
    
    for (const FileGroupEntry &entry : m_groupedFiles) {
        if (!entry.isProcessed && m_selectedIds.contains(entry.primaryFileId)) {
            QVariantMap item;
            item["fileId"] = entry.primaryFileId;
            item["filename"] = entry.displayName;
            item["path"] = entry.currentPath;
            item["extensions"] = entry.extensions.join("/");
            item["allFileIds"] = QVariant::fromValue(entry.allFileIds);
            result.append(item);
        }
    }
    
    return result;
}

bool FileListModel::isSelected(int fileId) const
{
    return m_selectedIds.contains(fileId);
}

void FileListModel::updateCounts()
{
    m_unprocessedCount = 0;
    m_processedCount = 0;
    
    for (const FileGroupEntry &entry : m_groupedFiles) {
        if (entry.isProcessed) {
            m_processedCount++;
        } else {
            m_unprocessedCount++;
        }
    }
    
    emit countsChanged();
}

QString FileListModel::extractBaseName(const QString &filename) const
{
    // Remove extension and common suffixes like "(Track 01)"
    QFileInfo fi(filename);
    QString baseName = fi.completeBaseName();
    
    // Remove track indicators for multi-bin games
    static QRegularExpression trackPattern(R"(\s*\(Track\s*\d+\)$)", QRegularExpression::CaseInsensitiveOption);
    baseName.remove(trackPattern);
    
    return baseName.trimmed();
}

void FileListModel::groupFiles(const QList<FileRecord> &files, const QMap<int, Database::MatchResult> &matches)
{
    m_groupedFiles.clear();
    
    // Map to group files by their base name AND source location
    // Key: source path + base name - keeps archives separate from extracted folders
    QMap<QString, FileGroupEntry> groups;
    
    for (const FileRecord &file : files) {
        // Create a grouping key based on source location + base name
        // This keeps archive contents separate from extracted folder contents
        QFileInfo pathInfo(file.originalPath);
        QString baseName = extractBaseName(file.filename);
        QString groupKey = pathInfo.path() + "/" + baseName;
        
        if (!groups.contains(groupKey)) {
            FileGroupEntry entry;
            entry.primaryFileId = file.id;
            entry.displayName = baseName;
            entry.currentPath = file.currentPath;
            entry.systemId = file.systemId;
            entry.systemName = m_db->getSystemDisplayName(file.systemId);
            entry.lastModified = file.lastModified.toString(Qt::ISODate);
            entry.processingStatus = file.processingStatus;
            
            // Check for .remusmd marker file in directory (source of truth for processed state)
            // Marker file existence determines processing status, not database flag
            // This handles cases where user manually deletes extracted folders
            QString markerPath = pathInfo.path() + "/.remusmd";
            entry.isProcessed = QFile::exists(markerPath);
            
            groups[groupKey] = entry;
        }
        
        FileGroupEntry &entry = groups[groupKey];
        
        // Add extension if not already present
        QString ext = file.extension.toLower();
        if (!entry.extensions.contains(ext)) {
            entry.extensions.append(ext);
        }
        
        // Update totals
        entry.totalSize += file.fileSize;
        entry.fileCount++;
        entry.allFileIds.append(file.id);
        
        // Store hash data from primary file
        if (!file.crc32.isEmpty()) entry.crc32 = file.crc32;
        if (!file.md5.isEmpty()) entry.md5 = file.md5;
        if (!file.sha1.isEmpty()) entry.sha1 = file.sha1;
        
        // Check if this file has hash data
        if (!file.crc32.isEmpty() || !file.md5.isEmpty() || !file.sha1.isEmpty()) {
            entry.hasHashes = true;
        }
        
        // Check if file is inside an archive (original path contains archive extension)
        for (const QString &archiveExt : ARCHIVE_EXTENSIONS) {
            if (file.originalPath.contains(archiveExt, Qt::CaseInsensitive)) {
                entry.isInsideArchive = true;
                // Extract archive path (everything up to and including the archive extension)
                int pos = file.originalPath.indexOf(archiveExt, 0, Qt::CaseInsensitive);
                entry.archivePath = file.originalPath.left(pos + archiveExt.length());
                // Store the archive extension (normalize to lowercase)
                entry.archiveExtension = archiveExt.toLower();
                break;
            }
        }
        
        // Check if already CHD
        if (ext == ".chd") {
            entry.isAlreadyChd = true;
        }
        
        // Check if CHD candidate (has source disc extension)
        for (const QString &discExt : CHD_SOURCE_EXTENSIONS) {
            if (ext == discExt.toLower()) {
                entry.isChdCandidate = true;
                break;
            }
        }
        
        // Use primary file (.cue, .gdi, .m3u) as the main entry if found
        // BUT: Only update primaryFileId if this file has a match OR the current primary has no match
        // This ensures we don't lose match info when grouping duplicate files
        if (file.isPrimary || ext == ".cue" || ext == ".gdi" || ext == ".m3u") {
            bool currentPrimaryHasMatch = matches.contains(entry.primaryFileId);
            bool thisFileHasMatch = matches.contains(file.id);
            
            // Update primary if:
            // 1. This file has a match and current doesn't, OR
            // 2. Current primary has no match (safe to replace), OR
            // 3. This is the first file being added to the group
            if (thisFileHasMatch || !currentPrimaryHasMatch || entry.primaryFileId == file.id) {
                entry.primaryFileId = file.id;
                entry.displayName = baseName;
                entry.currentPath = file.currentPath;
            }
        }
        
        // Check for match info - use this file's match if it exists
        // OR if this is the current primary file
        if (matches.contains(file.id) && (entry.matchInfo.gameId == 0 || entry.primaryFileId == file.id)) {
            const Database::MatchResult &match = matches[file.id];
            entry.matchInfo.gameId = match.gameId;
            entry.matchInfo.confidence = static_cast<int>(match.confidence);
            entry.matchInfo.matchMethod = match.matchMethod;
            entry.matchInfo.matchedTitle = match.gameTitle;
            entry.matchInfo.publisher = match.publisher;
            entry.matchInfo.developer = match.developer;
            entry.matchInfo.year = match.releaseYear;
            entry.matchInfo.description = match.description;
            entry.matchInfo.genre = match.genre;
            entry.matchInfo.players = match.players;
            entry.matchInfo.region = match.region;
            entry.matchInfo.rating = match.rating;
            entry.matchInfo.isConfirmed = match.isConfirmed;
            entry.matchInfo.isRejected = match.isRejected;
        }
    }
    
    // Sort extensions for consistent display (primary formats first)
    auto sortExtensions = [](QStringList &exts) {
        // Priority order for display
        QMap<QString, int> priority = {
            {".cue", 0}, {".gdi", 1}, {".m3u", 2}, {".iso", 3}, {".chd", 4},
            {".bin", 10}, {".img", 11}, {".raw", 12}  // Data files lower priority
        };
        
        std::sort(exts.begin(), exts.end(), [&priority](const QString &a, const QString &b) {
            int pa = priority.value(a.toLower(), 5);  // Default priority 5
            int pb = priority.value(b.toLower(), 5);
            if (pa != pb) return pa < pb;
            return a < b;  // Alphabetical if same priority
        });
    };
    
    // Convert map to list and finalize workflow states
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        FileGroupEntry &entry = it.value();
        sortExtensions(entry.extensions);
        
        // === Determine workflow states ===
        
        // Extraction state - check if file is still inside archive or has been extracted
        if (entry.isInsideArchive) {
            // Check if current_path still points to an archive or to extracted file
            bool stillInArchive = false;
            for (const QString &archiveExt : ARCHIVE_EXTENSIONS) {
                if (entry.currentPath.endsWith(archiveExt, Qt::CaseInsensitive)) {
                    stillInArchive = true;
                    break;
                }
            }
            
            if (stillInArchive) {
                entry.extractionState = WorkflowState::NeedsAction;
            } else {
                entry.extractionState = WorkflowState::Complete;
            }
        } else {
            entry.extractionState = WorkflowState::NotApplicable;
        }
        
        // CHD state
        if (entry.isAlreadyChd) {
            entry.chdState = WorkflowState::Complete;
        } else if (entry.isChdCandidate && DISC_SYSTEMS.contains(entry.systemId)) {
            entry.chdState = WorkflowState::NeedsAction;
        } else {
            entry.chdState = WorkflowState::NotApplicable;
        }
        
        // Hash state
        if (entry.hasHashes) {
            entry.hashState = WorkflowState::Complete;
        } else {
            entry.hashState = WorkflowState::NeedsAction;
        }
        
        // Match state
        if (entry.matchInfo.gameId > 0) {
            if (entry.matchInfo.isConfirmed) {
                entry.matchState = WorkflowState::Complete;
            } else if (entry.matchInfo.isRejected) {
                entry.matchState = WorkflowState::NeedsAction;  // Needs re-match
            } else {
                // Has match but not confirmed - show as needing confirmation
                entry.matchState = WorkflowState::NeedsAction;
            }
        } else {
            entry.matchState = WorkflowState::NeedsAction;
        }
        
        m_groupedFiles.append(entry);
    }
    
    qDebug() << "FileListModel: Grouped" << files.count() << "files into" << m_groupedFiles.count() << "entries";
}

void FileListModel::loadFiles()
{
    if (!m_db) {
        qWarning() << "FileListModel: No database set";
        return;
    }
    
    beginResetModel();
    
    // Load all files from database
    m_files = m_db->getExistingFiles();
    
    // Load all matches for workflow state
    QMap<int, Database::MatchResult> matches = m_db->getAllMatches();
    
    // DEBUG: Log which file IDs have matches
    qDebug() << "FileListModel: getAllMatches() returned" << matches.count() << "matches for file IDs:" << matches.keys();
    for (auto it = matches.begin(); it != matches.end(); ++it) {
        qDebug() << "  File ID" << it.key() << "-> Game:" << it.value().gameTitle 
                 << "Publisher:" << it.value().publisher 
                 << "Confidence:" << it.value().confidence;
    }
    
    // Apply system filter if set
    if (!m_systemFilter.isEmpty()) {
        QList<FileRecord> filtered;
        int systemId = m_systemFilter.toInt();
        for (const FileRecord &file : m_files) {
            if (file.systemId == systemId) {
                filtered.append(file);
            }
        }
        m_files = filtered;
    }
    
    // Apply matched filter
    if (m_showMatchedOnly) {
        QList<FileRecord> filtered;
        for (const FileRecord &file : m_files) {
            if (matches.contains(file.id)) {
                filtered.append(file);
            }
        }
        m_files = filtered;
    }
    
    // Group files by game with match info
    groupFiles(m_files, matches);
    
    // Update processed/unprocessed counts
    updateCounts();
    
    qDebug() << "FileListModel::loadFiles() loaded" << m_files.count() << "files into" 
             << m_groupedFiles.count() << "groups"
             << "(" << m_unprocessedCount << "unprocessed," << m_processedCount << "processed)";
    for (const FileGroupEntry &entry : m_groupedFiles) {
        qDebug() << "  -" << entry.displayName << "(" << entry.extensions.join("/") << ")" 
                 << entry.fileCount << "files"
                 << "hash:" << (entry.hasHashes ? "yes" : "no")
                 << "match:" << (entry.matchInfo.gameId > 0 ? entry.matchInfo.matchedTitle : "none")
                 << "processed:" << (entry.isProcessed ? "yes" : "no");
    }
    
    endResetModel();
    emit countChanged();
}

} // namespace Remus
