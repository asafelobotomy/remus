#include "patch_controller.h"

#include <QFileInfo>
#include <QSettings>

#include "app_controller.h"
#include "settings_controller.h"
#include "../../core/constants/constants.h"
#include "../../core/hasher.h"
#include "../../core/patched_rom_parser.h"
#include "../../services/patch_service.h"

namespace Remus {

namespace {

    bool persistAppliedPatchLineage(Database *db, const QString &basePath, const QString &patchPath,
        const QString &outputPath, const PatchInfo &patchInfo) {
        if (db == nullptr) {
            return false;
        }

        Hasher hasher;
        const HashResult baseHashes = hasher.calculateHashes(basePath);
        const HashResult outputHashes = hasher.calculateHashes(outputPath);
        if (!baseHashes.success || !outputHashes.success) {
            return false;
        }

        const PatchedRomInfo outputInfo = PatchedRomParser::parse(QFileInfo(outputPath).completeBaseName());
        const PatchedRomInfo patchNameInfo = PatchedRomParser::parse(QFileInfo(patchPath).completeBaseName());

        AppliedPatchRecord record;
        record.basePath = basePath;
        record.outputPath = outputPath;
        record.patchPath = patchPath;
        record.patchFormat = patchInfo.formatName;
        record.baseTitle
            = !outputInfo.baseTitle.isEmpty() ? outputInfo.baseTitle : QFileInfo(basePath).completeBaseName();
        record.patchName = !outputInfo.patchName.isEmpty() ? outputInfo.patchName : patchNameInfo.patchName;
        record.fileType
            = !Constants::FileTypes::isOfficial(outputInfo.fileType) ? outputInfo.fileType : Constants::FileTypes::HACK;
        record.sourceChecksum = patchInfo.sourceChecksum;
        record.targetChecksum = patchInfo.targetChecksum;
        record.patchChecksum = patchInfo.patchChecksum;
        record.baseCrc32 = baseHashes.crc32;
        record.baseMd5 = baseHashes.md5;
        record.baseSha1 = baseHashes.sha1;
        record.outputCrc32 = outputHashes.crc32;
        record.outputMd5 = outputHashes.md5;
        record.outputSha1 = outputHashes.sha1;
        return db->insertAppliedPatch(record);
    }

} // namespace

PatchController::PatchController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
    , m_patchService(new PatchService()) {
    updateToolStatus();
}

PatchController::~PatchController() {
    delete m_patchService;
}

bool PatchController::applyPatch(const QString &basePath, const QString &patchPath, const QString &outputPath) {
    if (m_patching) {
        return false;
    }

    applyToolPaths();
    const QString resolvedBasePath = basePath.isEmpty() && m_appController != nullptr
        ? m_appController->selectedFile().value(QStringLiteral("path")).toString()
        : basePath;

    const PatchInfo info = m_patchService->detectFormat(patchPath);
    if (!info.valid || !m_patchService->isFormatSupported(info.format)) {
        return false;
    }

    m_patching = true;
    m_progress = 0;
    m_currentOperation = QStringLiteral("Applying %1 patch").arg(info.formatName);
    emit patchingChanged();
    emit progressChanged();
    emit currentOperationChanged();

    const PatchResult result = m_patchService->apply(resolvedBasePath, info, outputPath, [this](int percent) {
        m_progress = percent;
        emit progressChanged();
    });

    m_patching = false;
    m_progress = 100;
    emit patchingChanged();
    emit progressChanged();

    if (!result.success) {
        m_currentOperation = result.error;
        emit currentOperationChanged();
        return false;
    }

    persistAppliedPatchLineage(
        m_appController ? m_appController->database() : nullptr, resolvedBasePath, patchPath, result.outputPath, info);
    m_currentOperation = QStringLiteral("Created %1").arg(result.outputPath);
    emit currentOperationChanged();
    emit libraryChanged();
    return true;
}

bool PatchController::createPatch(
    const QString &originalPath, const QString &modifiedPath, const QString &patchPath, const QString &format) {
    applyToolPaths();
    return m_patchService->createPatch(originalPath, modifiedPath, patchPath, stringToFormat(format));
}

void PatchController::checkTools() {
    updateToolStatus();
}

void PatchController::updateToolStatus() {
    const QMap<QString, bool> tools = m_patchService->getToolStatus();
    m_toolStatus.insert(QStringLiteral("flips"), tools.value(QStringLiteral("flips"), false));
    m_toolStatus.insert(QStringLiteral("xdelta3"), tools.value(QStringLiteral("xdelta3"), false));
    m_toolStatus.insert(QStringLiteral("ips"), true);
    m_toolStatus.insert(QStringLiteral("bps"), tools.value(QStringLiteral("flips"), false));
    m_toolStatus.insert(QStringLiteral("ups"), tools.value(QStringLiteral("flips"), false));
    m_toolStatus.insert(QStringLiteral("ppf"), tools.value(QStringLiteral("ppf"), false));
    emit toolStatusChanged();
}

void PatchController::applyToolPaths() {
    QSettings settings(
        QString::fromLatin1(Constants::SETTINGS_ORGANIZATION), QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    const QString flipsPath = settings.value(QString::fromLatin1(GuiSettings::FLIPS_PATH)).toString().trimmed();
    const QString xdeltaPath = settings.value(QString::fromLatin1(GuiSettings::XDELTA3_PATH)).toString().trimmed();
    const QString ppfPath = settings.value(QString::fromLatin1(GuiSettings::PPF_PATH)).toString().trimmed();

    if (!flipsPath.isEmpty()) {
        m_patchService->setFlipsPath(flipsPath);
    }
    if (!xdeltaPath.isEmpty()) {
        m_patchService->setXdelta3Path(xdeltaPath);
    }
    if (!ppfPath.isEmpty()) {
        m_patchService->setPpfPath(ppfPath);
    }
    updateToolStatus();
}

PatchFormat PatchController::stringToFormat(const QString &format) const {
    const QString normalized = format.trimmed().toLower();
    if (normalized == QStringLiteral("ips")) {
        return PatchFormat::IPS;
    }
    if (normalized == QStringLiteral("bps")) {
        return PatchFormat::BPS;
    }
    if (normalized == QStringLiteral("ups")) {
        return PatchFormat::UPS;
    }
    if (normalized == QStringLiteral("xdelta") || normalized == QStringLiteral("xdelta3")) {
        return PatchFormat::XDelta3;
    }
    if (normalized == QStringLiteral("ppf")) {
        return PatchFormat::PPF;
    }
    return PatchFormat::Unknown;
}

} // namespace Remus