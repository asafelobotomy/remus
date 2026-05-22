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

#include <algorithm>

namespace Remus {

namespace {

QString scanResultIdentifier(const ScanResult &result)
{
    if (result.isCompressed && !result.archivePath.isEmpty() && !result.archiveInternalPath.isEmpty()) {
        const QString normalized = ArchiveExtractor::normalizeArchiveMemberPath(result.archiveInternalPath);
        if (!normalized.isEmpty()) {
            return result.archivePath + QStringLiteral("::") + normalized;
        }
    }

    return QFileInfo(result.path).absoluteFilePath();
}

QString fileRecordIdentifier(const FileRecord &record)
{
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
    , m_detector(std::make_unique<SystemDetector>())
{
    m_scanner->setExtensions(m_detector->getAllExtensions());
}

LibraryService::~LibraryService()
{
}

QList<ScanResult> LibraryService::scanFilesystem(const QString &path,
                                                  ProgressCallback progressCb,
                                                  LogCallback logCb)
{
    if (logCb) logCb(QString("Scanning: %1").arg(path));

    // Track how many files have been found; total is unknown until the walk
    // finishes, so we pass 0 for 'total' during scanning (indeterminate).
    int foundCount = 0;
    int lastTotal  = 0;

    QMetaObject::Connection progConn, fileConn;
    if (progressCb) {
        progConn = QObject::connect(m_scanner.get(), &Scanner::scanProgress,
            [&, progressCb](int done, int total) {
                foundCount = done;
                lastTotal  = total > 0 ? total : 0;
                progressCb(foundCount, lastTotal, {});
            });
        fileConn = QObject::connect(m_scanner.get(), &Scanner::fileFound,
            [&, progressCb](const QString &p) {
                progressCb(++foundCount, lastTotal, p);
            });
    }

    QList<ScanResult> results = m_scanner->scan(path);

    if (progConn) QObject::disconnect(progConn);
    if (fileConn) QObject::disconnect(fileConn);

    if (!m_scanner->wasCancelled()) {
        if (progressCb) progressCb(results.size(), results.size(), {});
        if (logCb) logCb(QString("Scan complete: %1 files").arg(results.size()));
    } else {
        if (logCb) logCb("Scan cancelled");
    }

    return results;
}

int LibraryService::scan(const QString &path, Database *db,
                         ProgressCallback progressCb, LogCallback logCb,
                         int existingLibraryId)
{
    if (!db) {
        if (logCb) logCb("No database provided");
        return 0;
    }

    const QList<ScanResult> results = scanFilesystem(path, progressCb, logCb);

    if (m_scanner->wasCancelled()) {
        return 0;
    }

    int libraryId = existingLibraryId > 0 ? existingLibraryId : db->insertLibrary(path);
    if (libraryId == 0) {
        if (logCb) logCb("Failed to create library entry");
        return 0;
    }

    const int inserted = persistScanResults(results, libraryId, db);
    if (logCb) logCb(QString("Inserted %1 files into database").arg(inserted));
    return inserted;
}

void LibraryService::cancelScan()
{
    if (m_scanner) m_scanner->requestCancel();
}

bool LibraryService::wasCancelled() const
{
    return m_scanner && m_scanner->wasCancelled();
}

QVariantMap LibraryService::getStats(Database *db) const
{
    QVariantMap stats;
    if (!db) return stats;

    auto files = db->getAllFiles();
    int hashed = 0;
    for (const auto &f : files) {
        if (f.hashCalculated) hashed++;
    }
    stats["totalFiles"]  = files.size();
    stats["hashedFiles"] = hashed;
    return stats;
}

QVariantList LibraryService::getSystems(Database *db) const
{
    QVariantList list;
    if (!db) return list;

    // Collect unique system IDs from files
    auto files = db->getAllFiles();
    QSet<int> seen;
    for (const auto &f : files) {
        if (f.systemId > 0 && !seen.contains(f.systemId)) {
            seen.insert(f.systemId);
            QVariantMap m;
            m["id"]   = f.systemId;
            m["name"] = db->getSystemDisplayName(f.systemId);
            list.append(m);
        }
    }
    return list;
}

QString LibraryService::getFilePath(Database *db, int fileId) const
{
    if (!db) return {};
    return db->getFilePath(fileId);
}

bool LibraryService::removeLibrary(Database *db, int libraryId)
{
    if (!db) return false;
    return db->deleteLibrary(libraryId);
}

QStringList LibraryService::getAllExtensions() const
{
    return m_detector->getAllExtensions();
}

int LibraryService::persistScanResults(const QList<ScanResult> &results,
                                       int libraryId, Database *db,
                                       ProgressCallback progressCb)
{
    int inserted = 0;
    QHash<QString, int> insertedIds;

    for (const FileRecord &existing : db->getAllFiles()) {
        const QString identifier = fileRecordIdentifier(existing);
        if (!identifier.isEmpty()) {
            insertedIds.insert(identifier, existing.id);
        }
    }

    QList<ScanResult> orderedResults = results;
    std::stable_sort(orderedResults.begin(), orderedResults.end(),
                     [](const ScanResult &left, const ScanResult &right) {
        return static_cast<int>(!left.isPrimary) < static_cast<int>(!right.isPrimary);
    });

    QSqlDatabase sqlDb = db->database();
    const bool useTransaction = sqlDb.driver()->hasFeature(QSqlDriver::Transactions);
    if (useTransaction)
        sqlDb.transaction();

    for (const ScanResult &sr : orderedResults) {
        // Detect system — use internal archive path for compressed files
        const QString systemDetectPath = sr.isCompressed && !sr.archiveInternalPath.isEmpty()
            ? sr.archiveInternalPath
            : sr.path;
        QString systemName = m_detector->detectSystem(sr.extension, systemDetectPath);

        // For compressed disc images, stream the first 64 KB for magic-byte
        // detection. No temp directory or full extraction is needed, even for
        // large disc images.
        if (sr.isCompressed && !sr.archivePath.isEmpty()
            && DiscMagicDetector::isDiscImageExtension(sr.extension)) {
            const QString memberPath = sr.archiveInternalPath.isEmpty()
                ? sr.filename : sr.archiveInternalPath;
            const DiscHeaderInfo discInfo = DiscMagicDetector::detectFromArchive(
                sr.archivePath, memberPath, sr.fileSize);
            if (discInfo.detected && !discInfo.systemName.isEmpty()) {
                systemName = discInfo.systemName;
            }
        }

        int systemId = systemName.isEmpty() ? 0 : db->getSystemId(systemName);

        FileRecord rec;
        rec.libraryId          = libraryId;
        rec.originalPath       = sr.path;
        rec.currentPath        = sr.path;
        rec.filename           = sr.filename;
        rec.extension          = sr.extension;
        rec.fileSize           = sr.fileSize;
        rec.isCompressed       = sr.isCompressed;
        rec.archivePath        = sr.archivePath;
        rec.archiveInternalPath = sr.archiveInternalPath;
        rec.systemId           = systemId;
        rec.isPrimary          = sr.isPrimary;
        if (!sr.parentFilePath.isEmpty()) {
            rec.parentFileId = insertedIds.value(sr.parentFilePath);
        }
        rec.lastModified       = sr.lastModified;

        const int insertedId = db->insertFile(rec);
        if (insertedId > 0) {
            inserted++;
            insertedIds.insert(scanResultIdentifier(sr), insertedId);
        }

        if (progressCb)
            progressCb(inserted, orderedResults.size(), {});
    }

    if (useTransaction)
        sqlDb.commit();

    return inserted;
}

} // namespace Remus
