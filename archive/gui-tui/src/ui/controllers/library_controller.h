#pragma once

#include <QObject>
#include <QString>
#include "../../core/database.h"

namespace Remus {

class LibraryService;
class HashService;

/**
 * @brief Controller for library management operations
 * 
 * Handles scanning, hashing, and library maintenance.
 * Delegates to LibraryService and HashService for business logic.
 * Exposed to QML as a context property.
 */
class LibraryController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    Q_PROPERTY(int scanProgress READ scanProgress NOTIFY scanProgressChanged)
    Q_PROPERTY(int scanTotal READ scanTotal NOTIFY scanTotalChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanStatusChanged)
    Q_PROPERTY(bool hashing READ isHashing NOTIFY hashingChanged)
    
public:
    explicit LibraryController(Database *db, QObject *parent = nullptr);
    ~LibraryController() override;
    
    bool isScanning() const { return m_scanning; }
    bool isHashing() const { return m_hashing; }
    int scanProgress() const { return m_scanProgress; }
    int scanTotal() const { return m_scanTotal; }
    QString scanStatus() const { return m_scanStatus; }
    
    // Library operations
    Q_INVOKABLE void scanDirectory(const QString &path);
    Q_INVOKABLE void hashFiles();
    Q_INVOKABLE void hashFile(int fileId);
    Q_INVOKABLE void cancelScan();
    Q_INVOKABLE void removeLibrary(int libraryId);
    Q_INVOKABLE void refreshLibrary(int libraryId);
    Q_INVOKABLE void refreshList();  // Refresh the list without rescanning

    /**
     * @brief Remove a single file record from the library database.
     * Does NOT delete the file from disk — only removes the DB entry and match.
     */
    Q_INVOKABLE void removeFile(int fileId);
    
    // File queries
    Q_INVOKABLE QString getFilePath(int fileId);
    
    // Library queries
    Q_INVOKABLE QVariantMap getLibraryStats();
    Q_INVOKABLE QVariantList getSystems();
    
signals:
    void scanningChanged();
    void hashingChanged();
    void scanProgressChanged();
    void scanTotalChanged();
    void scanStatusChanged();
    
    void scanStarted();
    void scanCompleted(int filesFound);
    void scanError(const QString &error);
    
    void hashingStarted();
    void hashingProgress(int current, int total);
    void hashingCompleted(int filesHashed);
    
    void libraryUpdated();
    
private:
    Database *m_db;
    LibraryService *m_libraryService = nullptr;
    HashService    *m_hashService    = nullptr;
    
    bool m_scanning = false;
    bool m_hashing = false;
    int m_scanProgress = 0;
    int m_scanTotal = 0;
    QString m_scanStatus;
    int m_currentLibraryId = 0;
};

} // namespace Remus
