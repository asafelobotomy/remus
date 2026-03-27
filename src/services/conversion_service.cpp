#include "conversion_service.h"
#include "../core/database.h"

#include <QFileInfo>
#include <QObject>

namespace Remus {

namespace {

// RAII helper that wires a DiscConverter::conversionProgress signal to a callback
// and disconnects automatically on destruction.
class ScopedProgressConnection {
public:
    ScopedProgressConnection(DiscConverter *converter,
                             ConversionService::ProgressCallback cb)
    {
        if (cb && converter) {
            m_conn = QObject::connect(converter, &DiscConverter::conversionProgress,
                [cb](int pct, const QString &info) { cb(pct, info); });
        }
    }
    ~ScopedProgressConnection() { if (m_conn) QObject::disconnect(m_conn); }
    ScopedProgressConnection(const ScopedProgressConnection &) = delete;
    ScopedProgressConnection &operator=(const ScopedProgressConnection &) = delete;
private:
    QMetaObject::Connection m_conn;
};

} // anonymous namespace

ConversionService::ConversionService()
    : m_chdConverter(new CHDConverter())
    , m_rvzConverter(new RVZConverter())
    , m_csoConverter(new CSOConverter())
    , m_archiveExtractor(new ArchiveExtractor())
    , m_archiveCreator(new ArchiveCreator())
{
}

ConversionService::~ConversionService()
{
    delete m_chdConverter;
    delete m_rvzConverter;
    delete m_csoConverter;
    delete m_archiveExtractor;
    delete m_archiveCreator;
}

// ── CHD Conversion ──────────────────────────────────────────

ConversionResult ConversionService::convertToCHD(const QString &path,
                                                     CHDCodec codec,
                                                     const QString &outputPath,
                                                     ProgressCallback progressCb)
{
    QFileInfo fi(path);
    if (!fi.exists()) {
        ConversionResult r;
        r.error = "File not found: " + path;
        return r;
    }

    m_chdConverter->setCodec(codec);
    ScopedProgressConnection guard(m_chdConverter, progressCb);

    ConversionResult result;
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

    return result;
}

ConversionResult ConversionService::extractCHD(const QString &chdPath,
                                                   const QString &outputPath,
                                                   ProgressCallback progressCb)
{
    QFileInfo fi(chdPath);
    if (!fi.exists()) {
        ConversionResult r;
        r.error = "File not found: " + chdPath;
        return r;
    }

    ScopedProgressConnection guard(m_chdConverter, progressCb);
    return m_chdConverter->extractCHDToCue(chdPath, outputPath);
}

QList<ConversionResult> ConversionService::batchConvertToCHD(
    const QStringList &inputPaths,
    const QString &outputDir,
    CHDCodec codec,
    ProgressCallback progressCb)
{
    m_chdConverter->setCodec(codec);
    ScopedProgressConnection guard(m_chdConverter, progressCb);
    return m_chdConverter->batchConvert(inputPaths, outputDir);
}

VerifyResult ConversionService::verifyCHD(const QString &chdPath)
{
    return m_chdConverter->verifyCHD(chdPath);
}

CHDInfo ConversionService::getCHDInfo(const QString &chdPath)
{
    return m_chdConverter->getCHDInfo(chdPath);
}

// ── RVZ Conversion ────────────────────────────────────────

ConversionResult ConversionService::convertToRVZ(const QString &path,
                                                     RVZCompression compression,
                                                     const QString &outputPath,
                                                     ProgressCallback progressCb)
{
    QFileInfo fi(path);
    if (!fi.exists()) {
        ConversionResult r;
        r.error = "File not found: " + path;
        return r;
    }

    m_rvzConverter->setCompression(compression);
    ScopedProgressConnection guard(m_rvzConverter, progressCb);
    return m_rvzConverter->convertIsoToRVZ(path, outputPath);
}

ConversionResult ConversionService::extractRVZ(const QString &rvzPath,
                                                   const QString &outputPath,
                                                   ProgressCallback progressCb)
{
    QFileInfo fi(rvzPath);
    if (!fi.exists()) {
        ConversionResult r;
        r.error = "File not found: " + rvzPath;
        return r;
    }

    ScopedProgressConnection guard(m_rvzConverter, progressCb);
    return m_rvzConverter->extractRVZToIso(rvzPath, outputPath);
}

QList<ConversionResult> ConversionService::batchConvertToRVZ(
    const QStringList &inputPaths,
    const QString &outputDir,
    RVZCompression compression,
    ProgressCallback progressCb)
{
    m_rvzConverter->setCompression(compression);
    ScopedProgressConnection guard(m_rvzConverter, progressCb);
    return m_rvzConverter->batchConvert(inputPaths, outputDir);
}

VerifyResult ConversionService::verifyRVZ(const QString &rvzPath)
{
    return m_rvzConverter->verifyRVZ(rvzPath);
}

// ── CSO Conversion ────────────────────────────────────────

ConversionResult ConversionService::convertToCSO(const QString &path,
                                                     const QString &outputPath,
                                                     ProgressCallback progressCb)
{
    QFileInfo fi(path);
    if (!fi.exists()) {
        ConversionResult r;
        r.error = "File not found: " + path;
        return r;
    }

    ScopedProgressConnection guard(m_csoConverter, progressCb);
    return m_csoConverter->convertIsoToCSO(path, outputPath);
}

ConversionResult ConversionService::extractCSO(const QString &csoPath,
                                                   const QString &outputPath,
                                                   ProgressCallback progressCb)
{
    QFileInfo fi(csoPath);
    if (!fi.exists()) {
        ConversionResult r;
        r.error = "File not found: " + csoPath;
        return r;
    }

    ScopedProgressConnection guard(m_csoConverter, progressCb);
    return m_csoConverter->extractCSOToIso(csoPath, outputPath);
}

QList<ConversionResult> ConversionService::batchConvertToCSO(
    const QStringList &inputPaths,
    const QString &outputDir,
    ProgressCallback progressCb)
{
    ScopedProgressConnection guard(m_csoConverter, progressCb);
    return m_csoConverter->batchConvert(inputPaths, outputDir);
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

bool ConversionService::isDolphinToolAvailable() const
{
    return m_rvzConverter->isDolphinToolAvailable();
}

QString ConversionService::getDolphinToolVersion() const
{
    return m_rvzConverter->getDolphinToolVersion();
}

void ConversionService::setDolphinToolPath(const QString &path)
{
    m_rvzConverter->setDolphinToolPath(path);
}

bool ConversionService::isMaxcsoAvailable() const
{
    return m_csoConverter->isMaxcsoAvailable();
}

QString ConversionService::getMaxcsoVersion() const
{
    return m_csoConverter->getMaxcsoVersion();
}

void ConversionService::setMaxcsoPath(const QString &path)
{
    m_csoConverter->setMaxcsoPath(path);
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
    m_rvzConverter->cancel();
    m_csoConverter->cancel();
    m_archiveExtractor->cancel();
    m_archiveCreator->cancel();
}

bool ConversionService::isRunning() const
{
    return m_chdConverter->isRunning() || m_rvzConverter->isRunning() ||
           m_csoConverter->isRunning() || m_archiveExtractor->isRunning() ||
           m_archiveCreator->isRunning();
}

} // namespace Remus
