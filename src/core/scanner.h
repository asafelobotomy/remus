#ifndef REMUS_SCANNER_H
#define REMUS_SCANNER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <functional>
#include "archive_extractor.h"

namespace Remus {

/**
 * @brief Represents a scanned file before database insertion
 */
struct ScanResult {
    QString path;
    QString filename;
    QString extension;
    qint64 fileSize;
    QString detectedSystem;
    QDateTime lastModified;
    bool isPrimary = true;
    QString parentFilePath;  // For .bin files in .cue+.bin sets
    bool isCompressed = false;  // File is inside an archive
    QString archivePath;  // Path to archive containing this file
    QString archiveInternalPath;  // Path within archive (if compressed)
};

/**
 * @brief File scanner for ROM libraries
 * 
 * Recursively scans directories for ROM files, filtering by extension
 * and grouping multi-file sets (e.g., .cue+.bin).
 */
class Scanner : public QObject {
    Q_OBJECT

public:
    explicit Scanner(QObject *parent = nullptr);

    /**
     * @brief Scan a directory recursively
     * @param libraryPath Root directory to scan
     * @return List of scanned files
     */
    QList<ScanResult> scan(const QString &libraryPath);

    /**
     * @brief Set extensions to scan for
     * @param extensions List of extensions (e.g., ".nes", ".sfc")
     */
    void setExtensions(const QStringList &extensions);

    /**
     * @brief Enable/disable multi-file detection (.cue+.bin, etc.)
     */
    void setMultiFileDetection(bool enabled) { m_multiFileDetection = enabled; }

    /**
     * @brief Enable/disable archive scanning (.zip, .7z, .rar, etc.)
     */
    void setArchiveScanning(bool enabled) { m_archiveScanning = enabled; }

    /**
     * @brief Request cancellation of an active scan
     */
    void requestCancel() { m_cancelRequested = true; }

    /**
     * @brief Check if the last scan was cancelled
     */
    bool wasCancelled() const { return m_cancelled; }

signals:
    void scanStarted(const QString &path);
    void fileFound(const QString &path);
    void scanProgress(int filesProcessed, int totalFiles);
    void scanCompleted(int totalFiles);
    void scanError(const QString &error);

private:
    void scanDirectory(const QString &dirPath, QList<ScanResult> &results);
    bool isValidExtension(const QString &extension) const;
    bool isArchiveExtension(const QString &extension) const;
    bool isInExcludedDirectory(const QString &dirPath) const;
    ScanResult createScanResult(const QFileInfo &fileInfo);
    void processArchive(const QString &archivePath, QList<ScanResult> &results);
    void detectMultiFileSets(QList<ScanResult> &results);
    void linkBinToCue(QList<ScanResult> &results);
    void linkGdiToTracks(QList<ScanResult> &results);
    void linkCcdToImage(QList<ScanResult> &results);
    void linkMdsToMdf(QList<ScanResult> &results);

    QStringList m_extensions;
    bool m_multiFileDetection = true;
    bool m_archiveScanning = true;
    int m_filesProcessed = 0;
    bool m_cancelRequested = false;
    bool m_cancelled = false;
    ArchiveExtractor m_archiveExtractor;
};

} // namespace Remus

#endif // REMUS_SCANNER_H
