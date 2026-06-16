#include "app_controller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include "../../metadata/metadata_cache.h"
#include "../../metadata/provider_orchestrator.h"
#include "../../core/disc_set_utils.h"

namespace Remus {

AppController::AppController(QObject *parent)
    : QObject(parent) {
    rebuildOrchestrator();
}

AppController::~AppController() {
    closeLibrary();
}

bool AppController::openLibrary(const QString &dbPath) {
    const QString cleanedPath = QDir::cleanPath(dbPath.trimmed());
    if (cleanedPath.isEmpty()) {
        setStatusMessage(QStringLiteral("Select a database path first."));
        return false;
    }

    QFileInfo info(cleanedPath);
    const QString parentDir = info.absolutePath();
    if (!QDir().mkpath(parentDir)) {
        setStatusMessage(QStringLiteral("Cannot create parent directory: %1").arg(parentDir));
        return false;
    }

    m_database.close();
    if (!m_database.initialize(cleanedPath)) {
        setStatusMessage(QStringLiteral("Failed to open library database: %1").arg(cleanedPath));
        return false;
    }

    const bool pathChanged = m_libraryPath != cleanedPath;
    const bool wasClosed = !m_libraryOpen;
    m_libraryPath = cleanedPath;
    m_libraryOpen = true;
    m_selectedFileId = 0;
    m_selectedGameId = 0;
    rebuildOrchestrator();
    setStatusMessage(QStringLiteral("Library ready: %1").arg(QFileInfo(cleanedPath).fileName()));

    if (pathChanged) {
        emit libraryPathChanged();
    }
    if (wasClosed) {
        emit libraryOpenChanged();
    }
    emit selectedFileChanged();
    emit selectedGameChanged();
    emit libraryOpened();
    return true;
}

void AppController::closeLibrary() {
    const bool wasOpen = m_libraryOpen;
    m_database.close();
    m_cache.reset();
    m_orchestrator.reset();
    m_libraryPath.clear();
    m_libraryOpen = false;
    m_selectedFileId = 0;
    m_selectedGameId = 0;

    if (wasOpen) {
        emit libraryPathChanged();
        emit libraryOpenChanged();
        emit selectedFileChanged();
        emit selectedGameChanged();
        emit orchestratorChanged();
        emit libraryClosed();
    }
}

bool AppController::eraseLibraryDatabase(bool eraseFiles, bool eraseMatchData, bool eraseApiCache, bool eraseArtwork) {
    if (!m_libraryOpen) {
        setStatusMessage(QStringLiteral("No library is open."));
        return false;
    }

    if (!eraseFiles && !eraseMatchData && !eraseApiCache && !eraseArtwork) {
        setStatusMessage(QStringLiteral("Nothing selected to erase."));
        return false;
    }

    QSqlDatabase db = m_database.database();
    if (!db.transaction()) {
        setStatusMessage(QStringLiteral("Failed to start database transaction."));
        return false;
    }

    QSqlQuery q(db);
    QStringList erased;

    if (eraseFiles) {
        // Deleting libraries cascades to files → matches, undo_queue (FK SET NULL).
        // Games and applied_patches are not cascade-linked so we remove them explicitly.
        if (!q.exec(QStringLiteral("DELETE FROM applied_patches")) || !q.exec(QStringLiteral("DELETE FROM libraries"))
            || // cascade: files → matches; undo_queue FK SET NULL
            !q.exec(QStringLiteral("DELETE FROM undo_queue")) || !q.exec(QStringLiteral("DELETE FROM games"))) {
            db.rollback();
            setStatusMessage(QStringLiteral("Erase failed: %1").arg(q.lastError().text()));
            return false;
        }
        erased << QStringLiteral("file records");
    } else if (eraseMatchData) {
        // Keep file records but strip match results and game metadata.
        if (!q.exec(QStringLiteral("DELETE FROM matches")) || !q.exec(QStringLiteral("DELETE FROM games"))) {
            db.rollback();
            setStatusMessage(QStringLiteral("Erase failed: %1").arg(q.lastError().text()));
            return false;
        }
        erased << QStringLiteral("match results");
    }

    if (eraseApiCache) {
        if (!q.exec(QStringLiteral("DELETE FROM cache"))) {
            db.rollback();
            setStatusMessage(QStringLiteral("Erase failed: %1").arg(q.lastError().text()));
            return false;
        }
        erased << QStringLiteral("API cache");
    }

    if (!db.commit()) {
        db.rollback();
        setStatusMessage(QStringLiteral("Failed to commit erase transaction."));
        return false;
    }

    // Post-commit side effects — these cannot be rolled back, so they run only after a successful commit.
    if (eraseFiles) {
        emit libraryDatabaseErased();
    }

    if (eraseApiCache) {
        const QString modCacheDir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                                        .filePath(QStringLiteral("mod_catalog_cache"));
        QDir(modCacheDir).removeRecursively();
    }

    if (eraseArtwork) {
        emit artworkCacheEraseRequested();
        erased << QStringLiteral("artwork cache");
    }

    // Reset selection state and notify all views to reload from the now-empty DB.
    m_selectedFileId = 0;
    m_selectedGameId = 0;
    emit selectedFileChanged();
    emit selectedGameChanged();
    emit libraryOpened(); // reloads FileListModel, WorkflowController, etc.

    setStatusMessage(QStringLiteral("Erased: %1.").arg(erased.join(QStringLiteral(", "))));
    return true;
}

QString AppController::defaultLibraryPath() const {
    const QString documentsDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(documentsDir).filePath(QStringLiteral("remus-library.db"));
}

QVariantMap AppController::selectedFile() {
    QVariantMap result;
    if (!m_libraryOpen || m_selectedFileId <= 0) {
        return result;
    }

    const FileRecord file = m_database.getFileById(m_selectedFileId);
    if (file.id <= 0) {
        return result;
    }

    result.insert(QStringLiteral("id"), file.id);
    result.insert(QStringLiteral("libraryId"), file.libraryId);
    result.insert(QStringLiteral("path"), file.currentPath);
    result.insert(QStringLiteral("filename"), file.filename);
    result.insert(QStringLiteral("extension"), file.extension);
    result.insert(QStringLiteral("systemId"), file.systemId);
    result.insert(QStringLiteral("systemName"), systemName(file.systemId));
    result.insert(QStringLiteral("crc32"), file.crc32);
    result.insert(QStringLiteral("md5"), file.md5);
    result.insert(QStringLiteral("sha1"), file.sha1);
    result.insert(QStringLiteral("fileSize"), file.fileSize);
    result.insert(QStringLiteral("baseTitle"), file.baseTitle);
    result.insert(QStringLiteral("discSetKey"), file.discSetKey);
    result.insert(QStringLiteral("discNumber"), file.discNumber);
    result.insert(QStringLiteral("archivePath"), file.archivePath);
    result.insert(QStringLiteral("isCompressed"), file.isCompressed);

    // Pipeline stage flags and paths
    QSqlQuery flagsQ(m_database.database());
    flagsQ.prepare(QStringLiteral("SELECT is_converted, is_bundled, bundle_output_path FROM files WHERE id = ?"));
    flagsQ.addBindValue(m_selectedFileId);
    bool isConverted = false, isBundled = false;
    QString bundleOutputPath;
    if (flagsQ.exec() && flagsQ.next()) {
        isConverted = flagsQ.value(0).toBool();
        isBundled = flagsQ.value(1).toBool();
        bundleOutputPath = flagsQ.value(2).toString();
    }

    QSqlQuery orgQ(m_database.database());
    orgQ.prepare(QStringLiteral("SELECT new_path FROM undo_queue "
                                "WHERE file_id = ? AND undone = 0 "
                                "ORDER BY executed_at DESC LIMIT 1"));
    orgQ.addBindValue(m_selectedFileId);
    QString organizedPath;
    if (orgQ.exec() && orgQ.next())
        organizedPath = orgQ.value(0).toString();

    result.insert(QStringLiteral("originalPath"), file.originalPath);
    result.insert(QStringLiteral("originalExists"), QFileInfo::exists(file.originalPath));
    result.insert(QStringLiteral("currentExists"), QFileInfo::exists(file.currentPath));
    result.insert(QStringLiteral("isConverted"), isConverted);
    result.insert(QStringLiteral("isBundled"), isBundled);
    result.insert(QStringLiteral("isOrganized"), !organizedPath.isEmpty());
    result.insert(QStringLiteral("organizedPath"), organizedPath);
    result.insert(QStringLiteral("bundleOutputPath"), bundleOutputPath);

    if (!file.discSetKey.isEmpty()) {
        const QList<FileRecord> discMembers = m_database.getFilesByDiscSetKey(file.discSetKey);
        QVariantList members;
        for (const FileRecord &member : discMembers) {
            QVariantMap memberMap;
            memberMap.insert(QStringLiteral("fileId"), member.id);
            memberMap.insert(QStringLiteral("filename"), member.filename);
            memberMap.insert(QStringLiteral("path"), member.currentPath);
            memberMap.insert(QStringLiteral("discNumber"), member.discNumber);
            memberMap.insert(QStringLiteral("discLabel"),
                DiscSetUtils::discRowLabel(
                    DiscSetUtils::labelPath(member.currentPath, member.archivePath, member.archiveInternalPath,
                        member.filename),
                    member.discNumber));
            members.append(memberMap);
        }
        result.insert(QStringLiteral("discSetMembers"), members);
        result.insert(QStringLiteral("discSetMemberCount"), members.size());
    } else {
        result.insert(QStringLiteral("discSetMembers"), QVariantList());
        result.insert(QStringLiteral("discSetMemberCount"), 0);
    }

    return result;
}

QVariantMap AppController::selectedMatch() {
    QVariantMap result;
    if (!m_libraryOpen || m_selectedFileId <= 0) {
        return result;
    }

    const Database::MatchResult match = m_database.getMatchForFile(m_selectedFileId);
    if (match.matchId <= 0) {
        return result;
    }

    result.insert(QStringLiteral("matchId"), match.matchId);
    result.insert(QStringLiteral("fileId"), match.fileId);
    result.insert(QStringLiteral("gameId"), match.gameId);
    result.insert(QStringLiteral("systemId"), match.systemId);
    result.insert(QStringLiteral("systemName"), systemName(match.systemId));
    result.insert(QStringLiteral("title"), match.gameTitle);
    result.insert(QStringLiteral("confidence"), match.confidence);
    result.insert(QStringLiteral("method"), match.matchMethod);
    result.insert(QStringLiteral("publisher"), match.publisher);
    result.insert(QStringLiteral("developer"), match.developer);
    result.insert(QStringLiteral("description"), match.description);
    result.insert(QStringLiteral("genre"), match.genre);
    result.insert(QStringLiteral("players"), match.players);
    result.insert(QStringLiteral("region"), match.region);
    result.insert(QStringLiteral("releaseYear"), match.releaseYear);
    result.insert(QStringLiteral("rating"), match.rating);
    result.insert(QStringLiteral("confirmed"), match.isConfirmed);
    result.insert(QStringLiteral("rejected"), match.isRejected);
    return result;
}

QString AppController::systemName(int systemId) {
    if (!m_libraryOpen || systemId <= 0) {
        return QString();
    }

    return m_database.getSystemDisplayName(systemId);
}

void AppController::setCurrentView(int view) {
    const int clamped = qBound(static_cast<int>(LibraryView), view, static_cast<int>(SettingsView));
    if (m_currentView == clamped) {
        return;
    }

    m_currentView = clamped;
    emit currentViewChanged();
}

void AppController::setSelectedFileId(int fileId) {
    if (m_selectedFileId == fileId) {
        refreshSelectedMatch();
        return;
    }

    m_selectedFileId = fileId;
    emit selectedFileChanged();
    emit selectedFileDataChanged();
    refreshSelectedMatch();
}

void AppController::setStatusMessage(const QString &message) {
    if (m_statusMessage == message) {
        return;
    }

    m_statusMessage = message;
    emit statusMessageChanged();
}

void AppController::refreshSelectedMatch() {
    int nextGameId = 0;
    if (m_libraryOpen && m_selectedFileId > 0) {
        nextGameId = m_database.getMatchForFile(m_selectedFileId).gameId;
    }

    if (m_selectedGameId != nextGameId) {
        m_selectedGameId = nextGameId;
        emit selectedGameChanged();
    }

    // Always notify QML so metadata fields re-evaluate after any match DB change.
    emit selectedMatchDataChanged();
}

void AppController::refreshSelectedFile() {
    emit selectedFileDataChanged();
}

} // namespace Remus