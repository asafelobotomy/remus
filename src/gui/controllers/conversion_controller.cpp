#include "conversion_controller.h"

#include <QFileInfo>
#include <QSettings>

#include "app_controller.h"
#include "settings_controller.h"
#include "../../core/constants/constants.h"

namespace Remus {

ConversionController::ConversionController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
{
    refreshToolStatus();
}

void ConversionController::convertSelected(const QString &format, const QString &outputPath)
{
    if (m_converting) {
        setLastMessage(QStringLiteral("A conversion is already running."));
        return;
    }

    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastMessage(QStringLiteral("Open a library before converting files."));
        return;
    }

    const int fileId = m_appController->selectedFileId();
    if (fileId <= 0) {
        setLastMessage(QStringLiteral("Select a file first."));
        return;
    }

    applyToolPaths();

    const QString normalizedFormat = format.trimmed().isEmpty() ? m_targetFormat : format.trimmed().toUpper();
    const FileRecord file = m_appController->database()->getFileById(fileId);
    if (file.id <= 0) {
        setLastMessage(QStringLiteral("The selected file no longer exists in the database."));
        return;
    }

    m_converting = true;
    m_progress = 0;
    emit convertingChanged();
    emit progressChanged();

    ConversionResult result;
    if (normalizedFormat == QStringLiteral("CHD")) {
        result = m_conversionService.convertToCHD(file.currentPath, CHDCodec::Auto, outputPath, [this](int percent, const QString &) {
            m_progress = percent;
            emit progressChanged();
        });
    } else if (normalizedFormat == QStringLiteral("RVZ")) {
        result = m_conversionService.convertToRVZ(file.currentPath, RVZCompression::Auto, outputPath, [this](int percent, const QString &) {
            m_progress = percent;
            emit progressChanged();
        });
    } else if (normalizedFormat == QStringLiteral("CSO")) {
        result = m_conversionService.convertToCSO(file.currentPath, outputPath, [this](int percent, const QString &) {
            m_progress = percent;
            emit progressChanged();
        });
    } else if (normalizedFormat == QStringLiteral("WBFS")) {
        result = m_wbfsConverter.convertIsoToWbfs(file.currentPath, outputPath);
        m_progress = 100;
        emit progressChanged();
    } else if (normalizedFormat == QStringLiteral("PBP")) {
        result = m_pbpExporter.exportToPBP(file.currentPath, outputPath);
        m_progress = 100;
        emit progressChanged();
    } else {
        result.success = false;
        result.error = QStringLiteral("Unsupported format: %1").arg(normalizedFormat);
    }

    m_converting = false;
    emit convertingChanged();

    if (!result.success) {
        setLastMessage(result.error.isEmpty() ? QStringLiteral("Conversion failed.") : result.error);
        return;
    }

    m_compressionRatio = result.compressionRatio;
    m_lastOutputPath = result.outputPath;
    registerOutputFile(file, result.outputPath);
    setLastMessage(QStringLiteral("Created %1").arg(QFileInfo(result.outputPath).fileName()));
    emit conversionFinished();
    emit libraryChanged();
}

void ConversionController::refreshToolStatus()
{
    applyToolPaths();

    m_toolStatus.insert(QStringLiteral("chdman"), m_conversionService.isChdmanAvailable());
    m_toolStatus.insert(QStringLiteral("dolphinTool"), m_conversionService.isDolphinToolAvailable());
    m_toolStatus.insert(QStringLiteral("maxcso"), m_conversionService.isMaxcsoAvailable());
    m_toolStatus.insert(QStringLiteral("wit"), m_wbfsConverter.isWitAvailable());
    m_toolStatus.insert(QStringLiteral("psxpackager"), m_pbpExporter.isPSXPackagerAvailable());
    emit toolStatusChanged();
}

void ConversionController::setTargetFormat(const QString &format)
{
    const QString normalized = format.trimmed().toUpper();
    if (m_targetFormat == normalized) {
        return;
    }

    m_targetFormat = normalized;
    emit targetFormatChanged();
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

void ConversionController::registerOutputFile(const FileRecord &sourceFile, const QString &outputPath)
{
    if (m_appController == nullptr || outputPath.isEmpty()) {
        return;
    }

    QFileInfo info(outputPath);
    if (!info.exists()) {
        return;
    }

    FileRecord record;
    record.libraryId = sourceFile.libraryId;
    record.originalPath = outputPath;
    record.currentPath = outputPath;
    record.filename = info.fileName();
    record.extension = info.suffix().toLower();
    record.fileSize = info.size();
    record.systemId = sourceFile.systemId;
    record.baseTitle = info.completeBaseName();
    m_appController->database()->insertFile(record);
}

void ConversionController::setLastMessage(const QString &message)
{
    if (m_lastMessage == message) {
        return;
    }

    m_lastMessage = message;
    emit lastMessageChanged();
}

} // namespace Remus