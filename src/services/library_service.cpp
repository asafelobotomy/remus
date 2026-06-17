#include "library_service.h"

#include "../core/scanner.h"
#include "../core/system_detector.h"
#include "../core/disc_magic_detector.h"
#include "../core/archive_extractor.h"
#include "../core/database.h"

#include <QFileInfo>
#include <QHash>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QSqlQuery>

#include <algorithm>

namespace Remus {

namespace {

    constexpr int kPersistProgressStep = 50;

    QString scanResultIdentifier(const ScanResult &result) {
        if (result.isCompressed && !result.archivePath.isEmpty() && !result.archiveInternalPath.isEmpty()) {
            const QString normalized = ArchiveExtractor::normalizeArchiveMemberPath(result.archiveInternalPath);
            if (!normalized.isEmpty()) {
                return result.archivePath + QStringLiteral("::") + normalized;
            }
        }

        return QFileInfo(result.path).absoluteFilePath();
    }

    QString fileRecordIdentifier(const FileRecord &record) {
        if (!record.archiveInternalPath.isEmpty()) {
            const QString normalized = ArchiveExtractor::normalizeArchiveMemberPath(record.archiveInternalPath);
            const QString archivePath = !record.originalPath.isEmpty()
                ? QFileInfo(record.originalPath).absoluteFilePath()
                : QFileInfo(record.archivePath.isEmpty() ? record.currentPath : record.archivePath).absoluteFilePath();
            if (!normalized.isEmpty() && !archivePath.isEmpty()) {
                return archivePath + QStringLiteral("::") + normalized;
            }
        }

        const QString path = !record.originalPath.isEmpty() ? record.originalPath : record.currentPath;
        return QFileInfo(path).absoluteFilePath();
    }

}

LibraryService::LibraryService()
    : m_scanner(std::make_unique<Scanner>())
    , m_detector(std::make_unique<SystemDetector>()) {
    m_scanner->setExtensions(m_detector->getAllExtensions());
}

LibraryService::~LibraryService() { }

QList<ScanResult> LibraryService::scanFilesystem(const QString &path, ProgressCallback progressCb, LogCallback logCb) {
    if (logCb)
        logCb(QString("Scanning: %1").arg(path));

    int foundCount = 0;
    int lastTotal = 0;

    QMetaObject::Connection progConn;
    if (progressCb) {
        progConn = QObject::connect(m_scanner.get(), &Scanner::scanProgress, [&, progressCb](int done, int total) {
            foundCount = done;
            lastTotal = total > 0 ? total : 0;
            progressCb(foundCount, lastTotal, { });
        });
    }

    QList<ScanResult> results = m_scanner->scan(path);

    if (progConn)
        QObject::disconnect(progConn);

    if (!m_scanner->wasCancelled()) {
        if (progressCb)
            progressCb(results.size(), results.size(), { });
        if (logCb)
            logCb(QString("Scan complete: %1 files").arg(results.size()));
    } else {
        if (logCb)
            logCb("Scan cancelled");
    }

    return results;
}

void LibraryService::cancelScan() {
    if (m_scanner)
        m_scanner->requestCancel();
}

bool LibraryService::wasCancelled() const {
    return m_scanner && m_scanner->wasCancelled();
}

QString LibraryService::getFilePath(Database *db, int fileId) const {
    if (!db)
        return { };
    return db->getFilePath(fileId);
}

bool LibraryService::removeLibrary(Database *db, int libraryId) {
    if (!db)
        return false;
    return db->deleteLibrary(libraryId);
}

QStringList LibraryService::getAllExtensions() const {
    return m_detector->getAllExtensions();
}

int LibraryService::persistScanResults(
    const QList<ScanResult> &results, int libraryId, Database *db, ProgressCallback progressCb) {
    int inserted = 0;
    QHash<QString, int> insertedIds;

    {
        QSqlQuery existingQuery(db->database());
        if (existingQuery.exec(QStringLiteral(
                "SELECT id, original_path, current_path, archive_path, archive_internal_path FROM files"))) {
            while (existingQuery.next()) {
                FileRecord existing;
                existing.id = existingQuery.value(0).toInt();
                existing.originalPath = existingQuery.value(1).toString();
                existing.currentPath = existingQuery.value(2).toString();
                existing.archivePath = existingQuery.value(3).toString();
                existing.archiveInternalPath = existingQuery.value(4).toString();
                const QString identifier = fileRecordIdentifier(existing);
                if (!identifier.isEmpty())
                    insertedIds.insert(identifier, existing.id);
            }
        }
    }

    QList<ScanResult> orderedResults = results;
    std::stable_sort(orderedResults.begin(), orderedResults.end(), [](const ScanResult &left, const ScanResult &right) {
        return static_cast<int>(!left.isPrimary) < static_cast<int>(!right.isPrimary);
    });

    QSqlDatabase sqlDb = db->database();
    bool useTransaction = sqlDb.driver()->hasFeature(QSqlDriver::Transactions);
    if (useTransaction && !sqlDb.transaction()) {
        qWarning() << "LibraryService::persistScanResults: failed to start transaction, proceeding without";
        useTransaction = false;
    }

    for (const ScanResult &sr : orderedResults) {
        // Detect system — use internal archive path for compressed files
        const QString systemDetectPath
            = sr.isCompressed && !sr.archiveInternalPath.isEmpty() ? sr.archiveInternalPath : sr.path;
        QString systemName = m_detector->detectSystem(sr.extension, systemDetectPath);

        // For compressed disc images, stream the first 64 KB for magic-byte
        // detection. No temp directory or full extraction is needed, even for
        // large disc images.
        if (sr.isCompressed && !sr.archivePath.isEmpty() && DiscMagicDetector::isDiscImageExtension(sr.extension)) {
            const QString memberPath = sr.archiveInternalPath.isEmpty() ? sr.filename : sr.archiveInternalPath;
            const DiscHeaderInfo discInfo
                = DiscMagicDetector::detectFromArchive(sr.archivePath, memberPath, sr.fileSize);
            if (discInfo.detected && !discInfo.systemName.isEmpty()) {
                systemName = discInfo.systemName;
            }
        }

        int systemId = systemName.isEmpty() ? 0 : db->getSystemId(systemName);

        FileRecord rec;
        rec.libraryId = libraryId;
        rec.originalPath = sr.path;
        rec.currentPath = sr.path;
        rec.filename = sr.filename;
        rec.extension = sr.extension;
        rec.fileSize = sr.fileSize;
        rec.isCompressed = sr.isCompressed;
        rec.archivePath = sr.archivePath;
        rec.archiveInternalPath = sr.archiveInternalPath;
        rec.systemId = systemId;
        rec.isPrimary = sr.isPrimary;
        if (!sr.parentFilePath.isEmpty()) {
            rec.parentFileId = insertedIds.value(sr.parentFilePath);
        }
        rec.lastModified = sr.lastModified;

        const int insertedId = db->insertFile(rec);
        if (insertedId > 0) {
            inserted++;
            insertedIds.insert(scanResultIdentifier(sr), insertedId);
        }

        if (progressCb) {
            const bool finished = inserted >= orderedResults.size();
            if (finished || inserted % kPersistProgressStep == 0)
                progressCb(inserted, orderedResults.size(), { });
        }
    }

    if (progressCb && !orderedResults.isEmpty())
        progressCb(inserted, orderedResults.size(), { });

    if (useTransaction) {
        if (!sqlDb.commit()) {
            qWarning() << "LibraryService::persistScanResults: commit failed, rolling back";
            sqlDb.rollback();
            return 0;
        }
    }

    if (inserted > 0)
        db->rebuildDiscSetsForLibrary(libraryId);

    return inserted;
}

} // namespace Remus
