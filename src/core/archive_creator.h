#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QMap>

#include "archive_extractor.h" // reuse ArchiveFormat enum

namespace Remus {

struct CompressionResult {
    bool success = false;
    QString outputPath;
    QString error;
    qint64 originalSize = 0;
    qint64 compressedSize = 0;
    int filesCompressed = 0;
    int failedFiles = 0;
    QStringList inputFiles;
};

/**
 * @brief Archive creator supporting ZIP output via libarchive.
 *
 * No external tools required — uses libarchive's streaming ZIP writer.
 */
class ArchiveCreator : public QObject {
    Q_OBJECT

public:
    explicit ArchiveCreator(QObject *parent = nullptr);
    ~ArchiveCreator() override = default;

    QMap<ArchiveFormat, bool> getAvailableTools() const;
    bool canCompress(ArchiveFormat format) const;

    // No-op setters kept for API compatibility
    void setZipPath(const QString &) { }
    void setSevenZipPath(const QString &) { }

    CompressionResult compress(
        const QStringList &inputPaths, const QString &outputArchive, ArchiveFormat format = ArchiveFormat::ZIP);

    CompressionResult compressDirectoryContents(
        const QString &rootDir, const QString &outputArchive, ArchiveFormat format = ArchiveFormat::ZIP);

    QList<CompressionResult> batchCompress(
        const QStringList &dirs, const QString &outputDir, ArchiveFormat format = ArchiveFormat::ZIP);

    void cancel();
    bool isRunning() const {
        return m_running;
    }

signals:
    void compressionStarted(const QString &outputPath);
    void compressionProgress(int percent, const QString &info);
    void compressionCompleted(const CompressionResult &result);
    void batchProgress(int current, int total, const QString &currentFile);
    void errorOccurred(const QString &error);

private:
    struct ArchiveInputEntry {
        QString sourcePath;
        QString archivePath;
    };

    bool m_running = false;
    bool m_cancelled = false;

    CompressionResult compressFiles(const QList<ArchiveInputEntry> &entries, const QString &outputArchive);

    QStringList collectRelativeFilePaths(const QString &rootDir) const;
    qint64 calculateTotalSize(const QStringList &paths) const;
};

} // namespace Remus
