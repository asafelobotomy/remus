#include "conversion_controller.h"

#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>

#include "settings_controller.h"
#include "../../core/archive_extractor.h"
#include "../../core/constants/constants.h"

namespace Remus {

QString ConversionController::extractIfArchive(const QString &filePath,
                                               std::unique_ptr<QTemporaryDir> &tmpDirOut)
{
    ArchiveExtractor extractor;
    if (!extractor.canExtract(filePath))
        return filePath;

    auto tmp = std::make_unique<QTemporaryDir>();
    if (!tmp->isValid())
        return filePath;

    const ExtractionResult exResult = extractor.extract(filePath, tmp->path());
    if (!exResult.success || exResult.extractedFiles.isEmpty())
        return filePath;

    // Prefer the file with the most converter-friendly extension
    static const QStringList priority = {
        QStringLiteral("cue"), QStringLiteral("gdi"), QStringLiteral("iso"),
        QStringLiteral("bin"), QStringLiteral("img"), QStringLiteral("gcm"),
        QStringLiteral("toc"), QStringLiteral("nrg"), QStringLiteral("ccd")
    };
    for (const QString &ext : priority) {
        for (const QString &extracted : exResult.extractedFiles) {
            if (QFileInfo(extracted).suffix().toLower() == ext) {
                tmpDirOut = std::move(tmp);
                return extracted;
            }
        }
    }
    return filePath; // no recognizable ROM inside — pass through
}

QString ConversionController::resolveAutoFormat(const QString &extension)
{
    const QString ext = extension.toLower().remove('.');

    // Already compressed — skip to avoid double-conversion
    static const QSet<QString> skipExts = {
        QStringLiteral("chd"), QStringLiteral("rvz"), QStringLiteral("cso"),
        QStringLiteral("wbfs"), QStringLiteral("pbp"), QStringLiteral("zip"),
        QStringLiteral("7z"), QStringLiteral("rar")
    };
    if (skipExts.contains(ext)) {
        return QString();
    }

    // GameCube/Wii disc images → RVZ
    static const QSet<QString> rvzExts = {
        QStringLiteral("gcm")
    };
    if (rvzExts.contains(ext)) {
        return QStringLiteral("RVZ");
    }

    // CD/DVD-based disc images → CHD (most universal lossy-free format)
    static const QSet<QString> chdExts = {
        QStringLiteral("cue"), QStringLiteral("bin"), QStringLiteral("iso"),
        QStringLiteral("img"), QStringLiteral("gdi"), QStringLiteral("toc"),
        QStringLiteral("nrg"), QStringLiteral("ccd")
    };
    if (chdExts.contains(ext)) {
        return QStringLiteral("CHD");
    }

    return QString(); // Unsupported extension — skip
}

void ConversionController::applyToolPaths()
{
    QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
                       QString::fromLatin1(Constants::SETTINGS_APPLICATION));

    const QString chdmanPath = settings.value(QString::fromLatin1(GuiSettings::CHDMAN_PATH)).toString().trimmed();
    if (!chdmanPath.isEmpty()) {
        m_conversionService.setChdmanPath(chdmanPath);
    }

    const QString dolphinPath = settings.value(QString::fromLatin1(GuiSettings::DOLPHIN_TOOL_PATH)).toString().trimmed();
    if (!dolphinPath.isEmpty()) {
        m_conversionService.setDolphinToolPath(dolphinPath);
    }

    const QString maxcsoPath = settings.value(QString::fromLatin1(GuiSettings::MAXCSO_PATH)).toString().trimmed();
    if (!maxcsoPath.isEmpty()) {
        m_conversionService.setMaxcsoPath(maxcsoPath);
    }

    const QString witPath = settings.value(QString::fromLatin1(GuiSettings::WIT_PATH)).toString().trimmed();
    if (!witPath.isEmpty()) {
        m_wbfsConverter.setWitPath(witPath);
    }

    const QString psxPackagerPath = settings.value(QString::fromLatin1(GuiSettings::PSXPACKAGER_PATH)).toString().trimmed();
    if (!psxPackagerPath.isEmpty()) {
        m_pbpExporter.setPSXPackagerPath(psxPackagerPath);
    }
}

} // namespace Remus
