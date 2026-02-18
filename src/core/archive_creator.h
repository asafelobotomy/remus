#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QProcess>

#include "archive_extractor.h"  // reuse ArchiveFormat enum

namespace Remus {

/**
 * @brief Result of an archive compression operation
 */
struct CompressionResult {
    bool success = false;
    QString outputPath;
    QString error;
    qint64 originalSize = 0;    // Total size of input files
    qint64 compressedSize = 0;  // Size of output archive
    int filesCompressed = 0;
    QStringList inputFiles;     // List of files that were compressed
};

/**
 * @brief Archive creator supporting ZIP and 7z formats
 *
 * Uses external tools for compression:
 * - ZIP: zip (standard on most systems)
 * - 7z: 7z or 7za (7-zip command line)
 *
 * Mirrors ArchiveExtractor's pattern.
 */
class ArchiveCreator : public QObject {
    Q_OBJECT

public:
    explicit ArchiveCreator(QObject *parent = nullptr);
    ~ArchiveCreator() = default;

    /**
     * @brief Check which compression tools are available
     * @return Map of format -> available
     */
    QMap<ArchiveFormat, bool> getAvailableTools() const;

    /**
     * @brief Check if a specific format can be created
     */
    bool canCompress(ArchiveFormat format) const;

    /**
     * @brief Set custom path for compression tools
     */
    void setZipPath(const QString &path);
    void setSevenZipPath(const QString &path);

    /**
     * @brief Compress files into an archive
     * @param inputPaths   List of files/directories to compress
     * @param outputArchive  Path for the output archive
     * @param format       Archive format (ZIP or SevenZip)
     * @return Compression result
     */
    CompressionResult compress(const QStringList &inputPaths,
                               const QString &outputArchive,
                               ArchiveFormat format = ArchiveFormat::ZIP);

    /**
     * @brief Batch compress directories into individual archives
     * @param dirs       List of directories to compress (each becomes one archive)
     * @param outputDir  Output directory for archives
     * @param format     Archive format
     * @return List of compression results
     */
    QList<CompressionResult> batchCompress(const QStringList &dirs,
                                           const QString &outputDir,
                                           ArchiveFormat format = ArchiveFormat::ZIP);

    /**
     * @brief Cancel any running compression
     */
    void cancel();

    /**
     * @brief Check if compression is currently running
     */
    bool isRunning() const { return m_running; }

signals:
    void compressionStarted(const QString &outputPath);
    void compressionProgress(int percent, const QString &info);
    void compressionCompleted(const CompressionResult &result);
    void batchProgress(int current, int total, const QString &currentFile);
    void errorOccurred(const QString &error);

protected:
    // Virtual for test mocking
    struct ProcessResult {
        int exitCode = -1;
        QString stdOut;
        QString stdErr;
        bool timedOut = false;
    };

    virtual ProcessResult runProcess(const QString &program,
                                     const QStringList &args,
                                     int timeoutMs = 600000);

private:
    // ── Tool paths ─────────────────────────────────────────
    QString m_zipPath;
    QString m_sevenZipPath;

    // ── State ──────────────────────────────────────────────
    bool m_running = false;
    bool m_cancelled = false;
    QProcess *m_currentProcess = nullptr;

    // ── Helpers ────────────────────────────────────────────
    QString findTool(const QStringList &candidates) const;
    bool isToolAvailable(const QString &path) const;

    CompressionResult compressZip(const QStringList &inputPaths, const QString &outputArchive);
    CompressionResult compress7z(const QStringList &inputPaths, const QString &outputArchive);

    qint64 calculateTotalSize(const QStringList &paths) const;
};

} // namespace Remus
