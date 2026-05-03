#include "conversion_controller.h"

#include <QFileInfo>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>

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
    m_progressMessage = QStringLiteral("Converting \"%1\" to %2\u2026").arg(QFileInfo(file.currentPath).fileName(), normalizedFormat);
    emit convertingChanged();
    emit progressChanged();
    emit progressMessageChanged();

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
        m_progressMessage = result.error.isEmpty() ? QStringLiteral("Conversion failed.") : result.error;
        emit progressMessageChanged();
        setLastMessage(result.error.isEmpty() ? QStringLiteral("Conversion failed.") : result.error);
        return;
    }

    m_compressionRatio = result.compressionRatio;
    m_lastOutputPath = result.outputPath;
    registerOutputFile(file, result.outputPath);
    {
        QSqlQuery upd(m_appController->database()->database());
        upd.prepare(QStringLiteral("UPDATE files SET is_converted = 1 WHERE id = ?"));
        upd.addBindValue(file.id);
        upd.exec();
    }
    m_progressMessage = QStringLiteral("Created %1").arg(QFileInfo(result.outputPath).fileName());
    emit progressMessageChanged();
    setLastMessage(QStringLiteral("Created %1").arg(QFileInfo(result.outputPath).fileName()));
    emit conversionFinished();
    emit libraryChanged();
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

void ConversionController::convertAll(const QString &format, const QString &outputPath)
{
    if (m_converting) {
        setLastMessage(QStringLiteral("A conversion is already running."));
        return;
    }

    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastMessage(QStringLiteral("Open a library before converting files."));
        return;
    }

    applyToolPaths();

    const QString normalizedInput = format.trimmed().toUpper();
    const QList<FileRecord> files = m_appController->database()->getAllFiles();
    if (files.isEmpty()) {
        setLastMessage(QStringLiteral("No files in library to convert."));
        return;
    }

    m_converting = true;
    m_progress = 0;
    m_progressMessage = QStringLiteral("Converting files\u2026");
    emit convertingChanged();
    emit progressChanged();
    emit progressMessageChanged();

    int converted = 0;
    int skipped = 0;
    int failed = 0;
    const int total = files.size();

    for (int i = 0; i < total; ++i) {
        const FileRecord &file = files.at(i);

        const QString normalizedFormat = (normalizedInput == QStringLiteral("AUTO"))
            ? resolveAutoFormat(file.extension)
            : normalizedInput;

        m_progressMessage = QStringLiteral("Converting %1 / %2: \"%3\"\u2026")
                                .arg(i + 1).arg(total).arg(QFileInfo(file.currentPath).fileName());
        emit progressMessageChanged();

        if (normalizedFormat.isEmpty()) {
            ++skipped;
            m_progress = (i + 1) * 100 / total;
            emit progressChanged();
            continue;
        }

        const int capturedIndex = i;
        auto progress = [this, capturedIndex, total](int percent, const QString &) {
            m_progress = (capturedIndex * 100 + percent) / total;
            emit progressChanged();
        };

        ConversionResult result;
        if (normalizedFormat == QStringLiteral("CHD")) {
            result = m_conversionService.convertToCHD(file.currentPath, CHDCodec::Auto, outputPath, progress);
        } else if (normalizedFormat == QStringLiteral("RVZ")) {
            result = m_conversionService.convertToRVZ(file.currentPath, RVZCompression::Auto, outputPath, progress);
        } else if (normalizedFormat == QStringLiteral("CSO")) {
            result = m_conversionService.convertToCSO(file.currentPath, outputPath, progress);
        } else if (normalizedFormat == QStringLiteral("WBFS")) {
            result = m_wbfsConverter.convertIsoToWbfs(file.currentPath, outputPath);
        } else if (normalizedFormat == QStringLiteral("PBP")) {
            result = m_pbpExporter.exportToPBP(file.currentPath, outputPath);
        } else {
            ++skipped;
            continue;
        }

        m_progress = (i + 1) * 100 / total;
        emit progressChanged();

        if (result.success) {
            ++converted;
            registerOutputFile(file, result.outputPath);
            {
                QSqlQuery upd(m_appController->database()->database());
                upd.prepare(QStringLiteral("UPDATE files SET is_converted = 1 WHERE id = ?"));
                upd.addBindValue(file.id);
                upd.exec();
            }
        } else {
            ++failed;
        }
    }

    m_converting = false;
    m_progressMessage = QStringLiteral("Converted %1 | Skipped %2 | Failed %3").arg(converted).arg(skipped).arg(failed);
    emit convertingChanged();
    emit progressMessageChanged();

    setLastMessage(QStringLiteral("Converted %1 | Skipped %2 | Failed %3")
                       .arg(converted).arg(skipped).arg(failed));
    if (converted > 0) {
        emit conversionFinished();
        emit libraryChanged();
    }
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