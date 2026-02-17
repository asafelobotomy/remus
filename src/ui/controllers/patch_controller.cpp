#include "patch_controller.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include "../../services/patch_service.h"

namespace Remus {

PatchController::PatchController(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_patchService(new PatchService())
{
    updateToolStatus();
}

PatchController::~PatchController()
{
    delete m_patchService;
}

void PatchController::updateToolStatus()
{
    auto tools = m_patchService->getToolStatus();
    m_toolStatus["flips"] = tools.value("flips", false);
    m_toolStatus["xdelta3"] = tools.value("xdelta3", false);
    m_toolStatus["ips_builtin"] = tools.value("ips_builtin", true);
    
    // Derive format support
    m_toolStatus["ips"] = true;  // Always supported (builtin)
    m_toolStatus["bps"] = tools.value("flips", false);
    m_toolStatus["ups"] = tools.value("flips", false);
    m_toolStatus["xdelta"] = tools.value("xdelta3", false);
    
    emit toolStatusChanged();
}

QVariantMap PatchController::detectPatchFormat(const QString &patchPath)
{
    PatchInfo info = m_patchService->detectFormat(patchPath);
    
    QVariantMap result;
    result["path"] = info.path;
    result["format"] = PatchEngine::formatName(info.format);
    result["formatName"] = info.formatName;
    result["size"] = info.size;
    result["sourceChecksum"] = info.sourceChecksum;
    result["targetChecksum"] = info.targetChecksum;
    result["patchChecksum"] = info.patchChecksum;
    result["valid"] = info.valid;
    result["error"] = info.error;
    result["supported"] = m_patchService->isFormatSupported(info.format);
    
    return result;
}

bool PatchController::isFormatSupported(const QString &format)
{
    return m_patchService->isFormatSupported(stringToFormat(format));
}

QStringList PatchController::getSupportedFormats()
{
    return m_patchService->getSupportedFormats();
}

bool PatchController::applyPatch(const QString &basePath, const QString &patchPath,
                                  const QString &outputPath)
{
    if (m_patching) {
        emit patchError("Patch operation already in progress");
        return false;
    }

    // Detect patch format
    PatchInfo info = m_patchService->detectFormat(patchPath);
    if (!info.valid) {
        emit patchError(QString("Invalid patch file: %1").arg(info.error));
        return false;
    }

    if (!m_patchService->isFormatSupported(info.format)) {
        emit patchError(QString("Patch format %1 is not supported. Please install required tools.")
                       .arg(info.formatName));
        return false;
    }

    m_patching = true;
    m_cancelRequested = false;
    m_progress = 0;
    m_currentOperation = QString("Applying %1 patch...").arg(info.formatName);
    emit patchingChanged();
    emit progressChanged();
    emit currentOperationChanged();
    emit patchStarted();

    PatchResult result = m_patchService->apply(basePath, info, outputPath,
        [this](int percent) {
            m_progress = percent;
            emit progressChanged();
        });

    m_patching = false;
    m_progress = 100;
    emit patchingChanged();
    emit progressChanged();

    if (result.success) {
        m_currentOperation = "Patch applied successfully";
        emit currentOperationChanged();
        emit patchCompleted(result.outputPath);
        qInfo() << "Applied patch to" << result.outputPath;
        return true;
    } else {
        m_currentOperation = "Patch failed";
        emit currentOperationChanged();
        emit patchError(result.error);
        return false;
    }
}

void PatchController::cancelPatching()
{
    m_cancelRequested = true;
}

bool PatchController::createPatch(const QString &originalPath, const QString &modifiedPath,
                                   const QString &patchPath, const QString &format)
{
    if (m_patching) {
        emit createPatchError("Patch operation already in progress");
        return false;
    }

    PatchFormat fmt = stringToFormat(format);
    if (!m_patchService->isFormatSupported(fmt)) {
        emit createPatchError(QString("Format %1 is not supported for patch creation").arg(format));
        return false;
    }

    m_patching = true;
    m_progress = 0;
    m_currentOperation = QString("Creating %1 patch...").arg(format.toUpper());
    emit patchingChanged();
    emit progressChanged();
    emit currentOperationChanged();
    emit createPatchStarted();

    bool success = m_patchService->createPatch(originalPath, modifiedPath, patchPath, fmt);

    m_patching = false;
    m_progress = 100;
    emit patchingChanged();
    emit progressChanged();

    if (success) {
        m_currentOperation = "Patch created successfully";
        emit currentOperationChanged();
        emit createPatchCompleted(patchPath);
        qInfo() << "Created patch at" << patchPath;
        return true;
    } else {
        m_currentOperation = "Patch creation failed";
        emit currentOperationChanged();
        emit createPatchError("Failed to create patch file");
        return false;
    }
}

void PatchController::checkTools()
{
    updateToolStatus();
}

void PatchController::setFlipsPath(const QString &path)
{
    m_patchService->setFlipsPath(path);
    updateToolStatus();
}

void PatchController::setXdeltaPath(const QString &path)
{
    m_patchService->setXdelta3Path(path);
    updateToolStatus();
}

QString PatchController::getFlipsPath()
{
    return m_patchService->getFlipsPath();
}

QString PatchController::getXdeltaPath()
{
    return m_patchService->getXdelta3Path();
}

QString PatchController::generateOutputPath(const QString &basePath, const QString &patchPath)
{
    return PatchService::generateOutputPath(basePath, patchPath);
}

PatchFormat PatchController::stringToFormat(const QString &format)
{
    QString fmt = format.toLower();
    
    if (fmt == "ips") return PatchFormat::IPS;
    if (fmt == "bps") return PatchFormat::BPS;
    if (fmt == "ups") return PatchFormat::UPS;
    if (fmt == "xdelta3" || fmt == "xdelta") return PatchFormat::XDelta3;
    if (fmt == "ppf") return PatchFormat::PPF;
    
    return PatchFormat::Unknown;
}

} // namespace Remus
