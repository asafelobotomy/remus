#include "patch_controller.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>

namespace Remus {

PatchController::PatchController(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_engine(new PatchEngine(this))
{
    connect(m_engine, &PatchEngine::patchProgress,
            this, &PatchController::onPatchProgress);
    connect(m_engine, &PatchEngine::patchComplete,
            this, &PatchController::onPatchComplete);
    connect(m_engine, &PatchEngine::patchError,
            this, &PatchController::onPatchError);
    
    updateToolStatus();
}

void PatchController::updateToolStatus()
{
    auto tools = m_engine->checkToolAvailability();
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
    PatchInfo info = m_engine->detectFormat(patchPath);
    
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
    result["supported"] = m_engine->isFormatSupported(info.format);
    
    return result;
}

bool PatchController::isFormatSupported(const QString &format)
{
    return m_engine->isFormatSupported(stringToFormat(format));
}

QStringList PatchController::getSupportedFormats()
{
    QStringList formats;
    formats << "IPS";  // Always supported
    
    if (m_toolStatus.value("flips").toBool()) {
        formats << "BPS" << "UPS";
    }
    if (m_toolStatus.value("xdelta3").toBool()) {
        formats << "XDelta3";
    }
    
    return formats;
}

bool PatchController::applyPatch(const QString &basePath, const QString &patchPath,
                                  const QString &outputPath)
{
    if (m_patching) {
        emit patchError("Patch operation already in progress");
        return false;
    }

    // Detect patch format
    PatchInfo info = m_engine->detectFormat(patchPath);
    if (!info.valid) {
        emit patchError(QString("Invalid patch file: %1").arg(info.error));
        return false;
    }

    if (!m_engine->isFormatSupported(info.format)) {
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

    PatchResult result = m_engine->apply(basePath, info, outputPath);

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
    if (!m_engine->isFormatSupported(fmt)) {
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

    bool success = m_engine->createPatch(originalPath, modifiedPath, patchPath, fmt);

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
    m_engine->setFlipsPath(path);
    updateToolStatus();
}

void PatchController::setXdeltaPath(const QString &path)
{
    m_engine->setXdelta3Path(path);
    updateToolStatus();
}

QString PatchController::getFlipsPath()
{
    return m_engine->getFlipsPath();
}

QString PatchController::getXdeltaPath()
{
    return m_engine->getXdelta3Path();
}

QString PatchController::generateOutputPath(const QString &basePath, const QString &patchPath)
{
    QFileInfo baseInfo(basePath);
    QFileInfo patchInfo(patchPath);

    QString baseName = baseInfo.completeBaseName();
    QString patchName = patchInfo.completeBaseName();
    QString ext = baseInfo.suffix();

    QString outputName = QString("%1 [%2].%3")
        .arg(baseName)
        .arg(patchName)
        .arg(ext);

    return baseInfo.dir().filePath(outputName);
}

void PatchController::onPatchProgress(int percentage)
{
    m_progress = percentage;
    emit progressChanged();
}

void PatchController::onPatchComplete(const PatchResult &result)
{
    m_patching = false;
    emit patchingChanged();
    
    if (result.success) {
        m_currentOperation = "Complete";
        emit currentOperationChanged();
        emit patchCompleted(result.outputPath);
    } else {
        emit patchError(result.error);
    }
}

void PatchController::onPatchError(const QString &error)
{
    m_patching = false;
    m_currentOperation = "Failed";
    emit patchingChanged();
    emit currentOperationChanged();
    emit patchError(error);
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
