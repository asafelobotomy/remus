#include "library_controller.h"
#include <QDebug>
#include <QDir>
#include "../../core/logging_categories.h"

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
{
    m_scanner = new Scanner(this);
    m_hasher = new Hasher(this);
    
    // Set up scanner extensions
    m_scanner->setExtensions(m_systemDetector.getAllExtensions());
    
    // Connect scanner signals
    connect(m_scanner, &Scanner::fileFound, this, &LibraryController::onFileFound);
    connect(m_scanner, &Scanner::scanProgress, this, &LibraryController::onScanProgress);
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
    
    // Create or reuse library entry
    if (m_currentLibraryId == 0) {
        m_currentLibraryId = m_db->insertLibrary(path);
    }

    if (m_currentLibraryId == 0) {
        m_scanning = false;
        m_scanStatus = "Failed to create library entry";
        emit scanningChanged();
        emit scanStatusChanged();
        emit scanError("Failed to create library entry");
        return;
    }
    
    // Start async scan
    QMetaObject::invokeMethod(this, [this, path]() {
        QList<ScanResult> results = m_scanner->scan(path);

        if (m_scanner->wasCancelled()) {
            m_scanning = false;
            m_scanStatus = "Scan cancelled";
            emit scanningChanged();
            emit scanStatusChanged();
            emit scanError("Scan cancelled");
            m_currentLibraryId = 0;
            return;
        }
        
        // Insert files into database
        int inserted = 0;
        for (const ScanResult &result : results) {
            QString systemName = m_systemDetector.detectSystem(result.extension, result.path);
            int systemId = systemName.isEmpty() ? 0 : m_db->getSystemId(systemName);
            
            FileRecord record;
            record.libraryId = m_currentLibraryId;
            record.originalPath = result.path;
            record.currentPath = result.path;
            record.filename = result.filename;
            record.extension = result.extension;
            record.fileSize = result.fileSize;
            record.systemId = systemId;
            record.isPrimary = result.isPrimary;
            record.lastModified = result.lastModified;
            
            if (m_db->insertFile(record) > 0) {
                inserted++;
            }
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
        QList<FileRecord> files = m_db->getFilesWithoutHashes();
        int total = files.count();
        int hashed = 0;
        
        for (const FileRecord &file : files) {
            int headerSize = Hasher::detectHeaderSize(file.currentPath, file.extension);
            bool stripHeader = (headerSize > 0);
            
            HashResult result = m_hasher->calculateHashes(file.currentPath, stripHeader, headerSize);
            
            if (result.success) {
                m_db->updateFileHashes(file.id, result.crc32, result.md5, result.sha1);
                hashed++;
                emit hashingProgress(hashed, total);
            }
        }
        
        m_hashing = false;
        emit hashingChanged();
        emit hashingCompleted(hashed);
        emit libraryUpdated();
    }, Qt::QueuedConnection);
}

void LibraryController::hashFile(int fileId)
{
    FileRecord file = m_db->getFileById(fileId);
    if (file.id == 0) {
        qWarning() << "File not found:" << fileId;
        return;
    }
    
    int headerSize = Hasher::detectHeaderSize(file.currentPath, file.extension);
    bool stripHeader = (headerSize > 0);
    
    HashResult result = m_hasher->calculateHashes(file.currentPath, stripHeader, headerSize);
    
    if (result.success) {
        m_db->updateFileHashes(file.id, result.crc32, result.md5, result.sha1);
        qDebug() << "Hashed file:" << file.filename << "CRC32:" << result.crc32;
        emit libraryUpdated();
    } else {
        qWarning() << "Failed to hash file:" << file.filename;
    }
}

QString LibraryController::getFilePath(int fileId)
{
    return m_db->getFilePath(fileId);
}

void LibraryController::cancelScan()
{
    if (!m_scanning || !m_scanner) {
        return;
    }

    m_scanStatus = "Cancelling scan...";
    emit scanStatusChanged();
    m_scanner->requestCancel();
}

void LibraryController::removeLibrary(int libraryId)
{
    if (m_db->deleteLibrary(libraryId)) {
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
    QVariantMap stats;
    stats["totalFiles"] = m_db->getAllFiles().count();
    stats["hashedFiles"] = 0; // Would need query
    stats["matchedFiles"] = 0; // Would need query
    return stats;
}

QVariantList LibraryController::getSystems()
{
    QVariantList systems;
    QMap<QString, int> counts = m_db->getFileCountBySystem();
    
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        QVariantMap system;
        system["name"] = it.key();
        system["count"] = it.value();
        systems.append(system);
    }
    
    return systems;
}

void LibraryController::onFileFound(const QString &path)
{
    m_scanStatus = "Found: " + QFileInfo(path).fileName();
    emit scanStatusChanged();
}

void LibraryController::onScanProgress(int processed, int total)
{
    m_scanProgress = processed;
    m_scanTotal = total;
    emit scanProgressChanged();
    emit scanTotalChanged();
}

void LibraryController::onScanComplete()
{
    m_scanning = false;
    emit scanningChanged();
}

void LibraryController::refreshList()
{
    // Simply emit the signal to refresh the list without any scanning
    // The FileListModel is connected to this signal and will reload from database
    emit libraryUpdated();
}

} // namespace Remus
