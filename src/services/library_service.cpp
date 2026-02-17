#include "library_service.h"

#include "../core/scanner.h"
#include "../core/system_detector.h"
#include "../core/database.h"

#include <QFileInfo>

namespace Remus {

LibraryService::LibraryService()
    : m_scanner(new Scanner())
    , m_detector(new SystemDetector())
{
    m_scanner->setExtensions(m_detector->getAllExtensions());
}

LibraryService::~LibraryService()
{
    delete m_scanner;
    delete m_detector;
}

int LibraryService::scan(const QString &path, Database *db,
                         ProgressCallback progressCb, LogCallback logCb,
                         int existingLibraryId)
{
    if (!db) {
        if (logCb) logCb("No database provided");
        return 0;
    }

    if (logCb) logCb(QString("Scanning: %1").arg(path));

    // Wire scanner signals to callbacks (direct connections, same thread)
    QMetaObject::Connection progConn, fileConn;
    if (progressCb) {
        progConn = QObject::connect(m_scanner, &Scanner::scanProgress,
            [&](int done, int total) { progressCb(done, total, {}); });
        fileConn = QObject::connect(m_scanner, &Scanner::fileFound,
            [&](const QString &p) { progressCb(0, 0, p); });
    }

    QList<ScanResult> results = m_scanner->scan(path);

    // Disconnect temporary connections
    if (progConn) QObject::disconnect(progConn);
    if (fileConn) QObject::disconnect(fileConn);

    if (m_scanner->wasCancelled()) {
        if (logCb) logCb("Scan cancelled");
        return 0;
    }

    if (progressCb) progressCb(results.size(), results.size(), {});
    if (logCb) logCb(QString("Scan complete: %1 files").arg(results.size()));

    // Create or reuse library entry
    int libraryId = existingLibraryId > 0 ? existingLibraryId : db->insertLibrary(path);
    if (libraryId == 0) {
        if (logCb) logCb("Failed to create library entry");
        return 0;
    }

    int inserted = persistScanResults(results, libraryId, db);
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
                                       int libraryId, Database *db)
{
    int inserted = 0;
    for (const ScanResult &sr : results) {
        // Detect system — use internal archive path for compressed files
        const QString systemDetectPath = sr.isCompressed && !sr.archiveInternalPath.isEmpty()
            ? sr.archiveInternalPath
            : sr.path;
        QString systemName = m_detector->detectSystem(sr.extension, systemDetectPath);
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
        rec.lastModified       = sr.lastModified;

        if (db->insertFile(rec) > 0) {
            inserted++;
        }
    }
    return inserted;
}

} // namespace Remus
