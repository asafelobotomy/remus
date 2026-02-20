#include "library_controller.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include "../../core/logging_categories.h"
#include "../../services/library_service.h"
#include "../../services/hash_service.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logUi)
#define qInfo() qCInfo(logUi)
#define qWarning() qCWarning(logUi)
#define qCritical() qCCritical(logUi)

namespace Remus {

LibraryController::LibraryController(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_libraryService(new LibraryService())
    , m_hashService(new HashService())
{
}

LibraryController::~LibraryController()
{
    delete m_libraryService;
    delete m_hashService;
}

void LibraryController::scanDirectory(const QString &path)
{
    if (m_scanning) {
        qWarning() << "Scan already in progress";
        return;
    }
    
    QDir dir(path);
    if (!dir.exists()) {
        emit scanError("Directory does not exist: " + path);
        return;
    }
    
    m_scanning = true;
    m_scanProgress = 0;
    m_scanTotal = 0;
    m_scanStatus = "Scanning...";
    emit scanningChanged();
    emit scanStarted();
    
    int libId = m_currentLibraryId;
    
    // Start async scan via service
    QMetaObject::invokeMethod(this, [this, path, libId]() {
        int inserted = m_libraryService->scan(path, m_db,
            // Progress callback — update UI properties
            [this](int done, int total, const QString &p) {
                if (!p.isEmpty()) {
                    m_scanStatus = "Found: " + QFileInfo(p).fileName();
                    emit scanStatusChanged();
                }
                if (total > 0) {
                    m_scanProgress = done;
                    m_scanTotal = total;
                    emit scanProgressChanged();
                    emit scanTotalChanged();
                }
            },
            // Log callback
            [](const QString &) {},
            libId);

        if (m_libraryService->wasCancelled()) {
            m_scanning = false;
            m_scanStatus = "Scan cancelled";
            emit scanningChanged();
            emit scanStatusChanged();
            emit scanError("Scan cancelled");
            m_currentLibraryId = 0;
            return;
        }
        
        m_scanning = false;
        m_scanStatus = QString("Scan complete: %1 files").arg(inserted);
        emit scanningChanged();
        emit scanStatusChanged();
        qDebug() << "LibraryController: Emitting scanCompleted with" << inserted << "files";
        emit scanCompleted(inserted);
        emit libraryUpdated();
        m_currentLibraryId = 0;
    }, Qt::QueuedConnection);
}

void LibraryController::hashFiles()
{
    if (m_hashing) {
        return;
    }
    
    m_hashing = true;
    emit hashingChanged();
    emit hashingStarted();
    
    QMetaObject::invokeMethod(this, [this]() {
        int hashed = m_hashService->hashAll(m_db,
            [this](int done, int total, const QString &) {
                emit hashingProgress(done, total);
            });
        
        m_hashing = false;
        emit hashingChanged();
        emit hashingCompleted(hashed);
        emit libraryUpdated();
    }, Qt::QueuedConnection);
}

void LibraryController::hashFile(int fileId)
{
    bool ok = m_hashService->hashFile(m_db, fileId);
    if (ok) {
        FileRecord file = m_db->getFileById(fileId);
        qDebug() << "Hashed file:" << file.filename << "CRC32:" << file.crc32;
        emit libraryUpdated();
    } else {
        qWarning() << "Failed to hash file:" << fileId;
    }
}

QString LibraryController::getFilePath(int fileId)
{
    return m_libraryService->getFilePath(m_db, fileId);
}

void LibraryController::cancelScan()
{
    if (!m_scanning) {
        return;
    }

    m_scanStatus = "Cancelling scan...";
    emit scanStatusChanged();
    m_libraryService->cancelScan();
}

void LibraryController::removeLibrary(int libraryId)
{
    if (m_libraryService->removeLibrary(m_db, libraryId)) {
        emit libraryUpdated();
    } else {
        emit scanError("Failed to remove library");
    }
}

void LibraryController::refreshLibrary(int libraryId)
{
    if (m_scanning) {
        qWarning() << "Scan already in progress";
        return;
    }

    QString path = m_db->getLibraryPath(libraryId);
    if (path.isEmpty()) {
        emit scanError("Library not found");
        return;
    }

    if (!m_db->deleteFilesForLibrary(libraryId)) {
        emit scanError("Failed to clear library files");
        return;
    }

    m_currentLibraryId = libraryId;
    scanDirectory(path);
}

QVariantMap LibraryController::getLibraryStats()
{
    return m_libraryService->getStats(m_db);
}

QVariantList LibraryController::getSystems()
{
    return m_libraryService->getSystems(m_db);
}

void LibraryController::refreshList()
{
    emit libraryUpdated();
}

void LibraryController::removeFile(int fileId)
{
    if (fileId <= 0) {
        qWarning() << "removeFile: invalid fileId" << fileId;
        return;
    }

    if (m_db->removeFile(fileId)) {
        qDebug() << "Removed file record" << fileId;
        emit libraryUpdated();
    } else {
        qWarning() << "Failed to remove file record" << fileId;
    }
}

} // namespace Remus
