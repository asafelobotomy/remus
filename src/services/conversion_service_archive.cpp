#include "conversion_service.h"

#include "../core/database.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>

namespace Remus {

namespace {

    QString extractedRelativePath(const QString &outputDir, const QString &path) {
        return QDir(outputDir).relativeFilePath(path).replace('\\', '/');
    }

    QString resolveExtractedPath(
        const QString &extractionRoot, const QStringList &extractedFiles, const FileRecord &file) {
        const QString normalizedMember = ArchiveExtractor::normalizeArchiveMemberPath(file.archiveInternalPath);
        if (!normalizedMember.isEmpty()) {
            for (const QString &path : extractedFiles) {
                if (extractedRelativePath(extractionRoot, path) == normalizedMember) {
                    return path;
                }
            }
        }

        QString matchedByName;
        for (const QString &path : extractedFiles) {
            if (QFileInfo(path).fileName().compare(file.filename, Qt::CaseInsensitive) == 0) {
                if (!matchedByName.isEmpty()) {
                    return QString();
                }
                matchedByName = path;
            }
        }

        if (!matchedByName.isEmpty()) {
            return matchedByName;
        }

        const QString basenamePath = QDir(extractionRoot).filePath(file.filename);
        return QFileInfo::exists(basenamePath) ? basenamePath : QString();
    }

} // namespace

ExtractionResult ConversionService::extractArchive(
    const QString &archivePath, const QString &outputDir, ProgressCallback progressCb) {
    QFileInfo fi(archivePath);
    if (!fi.exists()) {
        ExtractionResult r;
        r.error = "File not found: " + archivePath;
        return r;
    }

    QMetaObject::Connection conn;
    if (progressCb) {
        conn = QObject::connect(m_archiveExtractor.get(), &ArchiveExtractor::extractionProgress,
            [&](int pct, const QString &info) { progressCb(pct, info); });
    }

    ExtractionResult result = m_archiveExtractor->extract(archivePath, outputDir, true);

    if (conn)
        QObject::disconnect(conn);
    return result;
}

ExtractionResult ConversionService::extractArchiveWithDbUpdate(
    const QString &archivePath, const QString &outputDir, Database *db, ProgressCallback progressCb) {
    ExtractionResult result = extractArchive(archivePath, outputDir, progressCb);

    if (result.success && db) {
        QList<FileRecord> files = db->getAllFiles();
        for (const FileRecord &file : files) {
            if (file.currentPath != archivePath && file.archivePath != archivePath) {
                continue;
            }

            const QString extractedPath = resolveExtractedPath(result.outputDir, result.extractedFiles, file);
            QFileInfo extractedInfo(extractedPath);
            if (!extractedInfo.exists()) {
                continue;
            }

            FileRecord updatedRecord = file;
            updatedRecord.currentPath = extractedPath;
            updatedRecord.filename = extractedInfo.fileName();
            updatedRecord.extension
                = extractedInfo.suffix().isEmpty() ? QString() : QStringLiteral(".") + extractedInfo.suffix().toLower();
            updatedRecord.fileSize = extractedInfo.size();
            updatedRecord.isCompressed = false;
            updatedRecord.archivePath.clear();
            updatedRecord.archiveInternalPath.clear();
            db->updateFileStorageState(updatedRecord);
        }
    }

    return result;
}

CompressionResult ConversionService::compressToArchive(
    const QStringList &inputPaths, const QString &outputArchive, ArchiveFormat format, ProgressCallback progressCb) {
    QMetaObject::Connection conn;
    if (progressCb) {
        conn = QObject::connect(m_archiveCreator.get(), &ArchiveCreator::compressionProgress,
            [&](int pct, const QString &info) { progressCb(pct, info); });
    }

    CompressionResult result = m_archiveCreator->compress(inputPaths, outputArchive, format);

    if (conn)
        QObject::disconnect(conn);
    return result;
}

QList<CompressionResult> ConversionService::batchCompressToArchive(
    const QStringList &dirs, const QString &outputDir, ArchiveFormat format, ProgressCallback progressCb) {
    QMetaObject::Connection conn;
    if (progressCb) {
        conn = QObject::connect(m_archiveCreator.get(), &ArchiveCreator::compressionProgress,
            [&](int pct, const QString &info) { progressCb(pct, info); });
    }

    QList<CompressionResult> results = m_archiveCreator->batchCompress(dirs, outputDir, format);

    if (conn)
        QObject::disconnect(conn);
    return results;
}

bool ConversionService::canCompress(ArchiveFormat format) const {
    return m_archiveCreator->canCompress(format);
}

QMap<ArchiveFormat, bool> ConversionService::getArchiveCompressionToolStatus() const {
    return m_archiveCreator->getAvailableTools();
}

} // namespace Remus