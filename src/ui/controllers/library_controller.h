#pragma once

#include <QObject>
#include <QString>
#include "../../core/database.h"
#include "../../core/scanner.h"
#include "../../core/hasher.h"
#include "../../core/system_detector.h"

namespace Remus {

/**
 * @brief Controller for library management operations
 * 
 * Handles scanning, hashing, and library maintenance.
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
    
private slots:
    void onFileFound(const QString &path);
    void onScanProgress(int processed, int total);
    void onScanComplete();
    
private:
    Database *m_db;
    Scanner *m_scanner = nullptr;
    Hasher *m_hasher = nullptr;
    SystemDetector m_systemDetector;
    
    bool m_scanning = false;
    bool m_hashing = false;
    int m_scanProgress = 0;
    int m_scanTotal = 0;
    QString m_scanStatus;
    int m_currentLibraryId = 0;
};

} // namespace Remus
