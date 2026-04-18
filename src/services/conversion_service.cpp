#include "conversion_service.h"
#include "../core/constants/files.h"

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
    } else if (ext == "iso" || ext == "img") {
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

    const QString ext = QStringLiteral(".") + fi.suffix().toLower();
    if (!Constants::Files::containsExtension(Constants::Files::CSO_SOURCE_EXTENSIONS, ext)) {
        ConversionResult r;
        r.error = "Unsupported file format: " + ext;
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

    const QString ext = QStringLiteral(".") + fi.suffix().toLower();
    if (ext != Constants::Files::CSO) {
        ConversionResult r;
        r.error = "Unsupported file format: " + ext;
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

} // namespace Remus
