#include "conversion_service.h"
#include "../core/database.h"

#include <QFileInfo>
#include <QObject>

namespace Remus {

ConversionService::ConversionService()
    : m_chdConverter(new CHDConverter())
    , m_archiveExtractor(new ArchiveExtractor())
    , m_archiveCreator(new ArchiveCreator())
{
}

ConversionService::~ConversionService()
{
    delete m_chdConverter;
    delete m_archiveExtractor;
    delete m_archiveCreator;
}

// ── CHD Conversion ──────────────────────────────────────────

CHDConversionResult ConversionService::convertToCHD(const QString &path,
                                                     CHDCodec codec,
                                                     const QString &outputPath,
                                                     ProgressCallback progressCb)
{
    QFileInfo fi(path);
    if (!fi.exists()) {
        CHDConversionResult r;
        r.error = "File not found: " + path;
        return r;
    }

    m_chdConverter->setCodec(codec);

    // Wire progress callback
    QMetaObject::Connection conn;
    if (progressCb) {
        conn = QObject::connect(m_chdConverter, &CHDConverter::conversionProgress,
            [&](int pct, const QString &info) { progressCb(pct, info); });
    }

    CHDConversionResult result;
    const QString ext = fi.suffix().toLower();

    if (ext == "cue") {
        result = m_chdConverter->convertCueToCHD(path, outputPath);
    } else if (ext == "iso") {
        result = m_chdConverter->convertIsoToCHD(path, outputPath);
    } else if (ext == "gdi") {
        result = m_chdConverter->convertGdiToCHD(path, outputPath);
    } else {
        result.error = "Unsupported file format: " + ext;
    }

    if (conn) QObject::disconnect(conn);
    return result;
}

CHDConversionResult ConversionService::extractCHD(const QString &chdPath,
                                                   const QString &outputPath,
                                                   ProgressCallback progressCb)
{
    QFileInfo fi(chdPath);
    if (!fi.exists()) {
        CHDConversionResult r;
        r.error = "File not found: " + chdPath;
        return r;
    }

    QMetaObject::Connection conn;
    if (progressCb) {
        conn = QObject::connect(m_chdConverter, &CHDConverter::conversionProgress,
            [&](int pct, const QString &info) { progressCb(pct, info); });
    }

    CHDConversionResult result = m_chdConverter->extractCHDToCue(chdPath, outputPath);

    if (conn) QObject::disconnect(conn);
    return result;
}

QList<CHDConversionResult> ConversionService::batchConvertToCHD(
    const QStringList &inputPaths,
    const QString &outputDir,
    CHDCodec codec,
    ProgressCallback progressCb)
{
    m_chdConverter->setCodec(codec);

    QMetaObject::Connection conn;
    if (progressCb) {
        conn = QObject::connect(m_chdConverter, &CHDConverter::conversionProgress,
            [&](int pct, const QString &info) { progressCb(pct, info); });
    }

    QList<CHDConversionResult> results = m_chdConverter->batchConvert(inputPaths, outputDir);

    if (conn) QObject::disconnect(conn);
    return results;
}

CHDVerifyResult ConversionService::verifyCHD(const QString &chdPath)
{
    return m_chdConverter->verifyCHD(chdPath);
}

CHDInfo ConversionService::getCHDInfo(const QString &chdPath)
{
    return m_chdConverter->getCHDInfo(chdPath);
}

// ── Archive Extraction ──────────────────────────────────────

ExtractionResult ConversionService::extractArchive(const QString &archivePath,
                                                    const QString &outputDir,
                                                    ProgressCallback progressCb)
{
    QFileInfo fi(archivePath);
    if (!fi.exists()) {
        ExtractionResult r;
        r.error = "File not found: " + archivePath;
        return r;
    }

    QMetaObject::Connection conn;
    if (progressCb) {
        conn = QObject::connect(m_archiveExtractor, &ArchiveExtractor::extractionProgress,
            [&](int pct, const QString &info) { progressCb(pct, info); });
    }

    ExtractionResult result = m_archiveExtractor->extract(archivePath, outputDir, true);

    if (conn) QObject::disconnect(conn);
    return result;
}

ExtractionResult ConversionService::extractArchiveWithDbUpdate(
    const QString &archivePath,
    const QString &outputDir,
    Database *db,
    ProgressCallback progressCb)
{
    ExtractionResult result = extractArchive(archivePath, outputDir, progressCb);

    if (result.success && db) {
        // Update database entries that reference this archive
        QList<FileRecord> files = db->getAllFiles();
        for (const FileRecord &file : files) {
            if (file.currentPath == archivePath || file.originalPath.contains(archivePath)) {
                QString extractedPath = outputDir + "/" + file.filename;
                QFileInfo extractedInfo(extractedPath);
                if (extractedInfo.exists()) {
                    db->updateFilePath(file.id, extractedPath);
                }
            }
        }
    }

    return result;
}

// ── Tool Status ─────────────────────────────────────────────

bool ConversionService::isChdmanAvailable() const
{
    return m_chdConverter->isChdmanAvailable();
}

QString ConversionService::getChdmanVersion() const
{
    return m_chdConverter->getChdmanVersion();
}

void ConversionService::setChdmanPath(const QString &path)
{
    m_chdConverter->setChdmanPath(path);
}

QMap<ArchiveFormat, bool> ConversionService::getArchiveToolStatus() const
{
    return m_archiveExtractor->getAvailableTools();
}

bool ConversionService::canExtract(const QString &path) const
{
    return m_archiveExtractor->canExtract(path);
}

// ── Archive Compression ────────────────────────────────────────

CompressionResult ConversionService::compressToArchive(const QStringList &inputPaths,
                                                        const QString &outputArchive,
                                                        ArchiveFormat format,
                                                        ProgressCallback progressCb)
{
    QMetaObject::Connection conn;
    if (progressCb) {
        conn = QObject::connect(m_archiveCreator, &ArchiveCreator::compressionProgress,
            [&](int pct, const QString &info) { progressCb(pct, info); });
    }

    CompressionResult result = m_archiveCreator->compress(inputPaths, outputArchive, format);

    if (conn) QObject::disconnect(conn);
    return result;
}

QList<CompressionResult> ConversionService::batchCompressToArchive(
    const QStringList &dirs,
    const QString &outputDir,
    ArchiveFormat format,
    ProgressCallback progressCb)
{
    QMetaObject::Connection conn;
    if (progressCb) {
        conn = QObject::connect(m_archiveCreator, &ArchiveCreator::compressionProgress,
            [&](int pct, const QString &info) { progressCb(pct, info); });
    }

    QList<CompressionResult> results = m_archiveCreator->batchCompress(dirs, outputDir, format);

    if (conn) QObject::disconnect(conn);
    return results;
}

bool ConversionService::canCompress(ArchiveFormat format) const
{
    return m_archiveCreator->canCompress(format);
}

QMap<ArchiveFormat, bool> ConversionService::getArchiveCompressionToolStatus() const
{
    return m_archiveCreator->getAvailableTools();
}

void ConversionService::cancel()
{
    m_chdConverter->cancel();
    m_archiveExtractor->cancel();
    m_archiveCreator->cancel();
}

bool ConversionService::isRunning() const
{
    return m_chdConverter->isRunning() || m_archiveExtractor->isRunning() || m_archiveCreator->isRunning();
}

} // namespace Remus
