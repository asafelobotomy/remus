#include "patch_service.h"

#include <QFileInfo>
#include <QObject>

namespace Remus {

PatchService::PatchService()
    : m_engine(new PatchEngine())
{
}

PatchService::~PatchService()
{
    delete m_engine;
}

// ── Patch Detection ─────────────────────────────────────────

PatchInfo PatchService::detectFormat(const QString &patchPath)
{
    return m_engine->detectFormat(patchPath);
}

bool PatchService::isFormatSupported(PatchFormat format) const
{
    return m_engine->isFormatSupported(format);
}

QStringList PatchService::getSupportedFormats() const
{
    QStringList formats;
    formats << "IPS"; // Always supported (builtin)

    auto tools = m_engine->checkToolAvailability();
    if (tools.value("flips", false)) {
        formats << "BPS" << "UPS";
    }
    if (tools.value("xdelta3", false)) {
        formats << "XDelta3";
    }
    return formats;
}

// ── Apply Patch ─────────────────────────────────────────────

PatchResult PatchService::apply(const QString &basePath,
                                const QString &patchPath,
                                const QString &outputPath,
                                ProgressCallback progressCb)
{
    PatchInfo info = m_engine->detectFormat(patchPath);
    return apply(basePath, info, outputPath, progressCb);
}

PatchResult PatchService::apply(const QString &basePath,
                                const PatchInfo &info,
                                const QString &outputPath,
                                ProgressCallback progressCb)
{
    PatchResult result;

    if (!info.valid) {
        result.error = QString("Invalid patch file: %1").arg(info.error);
        return result;
    }

    if (!m_engine->isFormatSupported(info.format)) {
        result.error = QString("Patch format %1 is not supported. Install required tools.")
                       .arg(info.formatName);
        return result;
    }

    // Wire progress callback
    QMetaObject::Connection conn;
    if (progressCb) {
        conn = QObject::connect(m_engine, &PatchEngine::patchProgress,
            [&](int pct) { progressCb(pct); });
    }

    result = m_engine->apply(basePath, info, outputPath);

    if (conn) QObject::disconnect(conn);
    return result;
}

QList<PatchResult> PatchService::batchApply(const QString &basePath,
                                            const QStringList &patchPaths,
                                            ProgressCallback progressCb,
                                            LogCallback logCb)
{
    QList<PatchResult> results;
    for (const QString &pp : patchPaths) {
        if (logCb) logCb(QString("Applying: %1").arg(QFileInfo(pp).fileName()));
        PatchResult r = apply(basePath, pp, {}, progressCb);
        results.append(r);
        if (!r.success && logCb) {
            logCb(QString("Failed: %1").arg(r.error));
        }
    }
    return results;
}

// ── Create Patch ────────────────────────────────────────────

bool PatchService::createPatch(const QString &originalPath,
                               const QString &modifiedPath,
                               const QString &patchPath,
                               PatchFormat format)
{
    return m_engine->createPatch(originalPath, modifiedPath, patchPath, format);
}

// ── Tool Management ─────────────────────────────────────────

QMap<QString, bool> PatchService::getToolStatus() const
{
    return m_engine->checkToolAvailability();
}

void PatchService::setFlipsPath(const QString &path)
{
    m_engine->setFlipsPath(path);
}

void PatchService::setXdelta3Path(const QString &path)
{
    m_engine->setXdelta3Path(path);
}

void PatchService::setPpfPath(const QString &path)
{
    m_engine->setPpfPath(path);
}

QString PatchService::getFlipsPath() const
{
    return m_engine->getFlipsPath();
}

QString PatchService::getXdelta3Path() const
{
    return m_engine->getXdelta3Path();
}

QString PatchService::getPpfPath() const
{
    return m_engine->getPpfPath();
}

QString PatchService::generateOutputPath(const QString &basePath,
                                         const QString &patchPath)
{
    QFileInfo baseInfo(basePath);
    QFileInfo patchInfo(patchPath);
    return baseInfo.absolutePath() + "/" +
           baseInfo.completeBaseName() + "_patched." +
           baseInfo.suffix();
}

} // namespace Remus
