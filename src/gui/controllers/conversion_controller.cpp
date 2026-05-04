#include "conversion_controller.h"

#include <memory>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "app_controller.h"

namespace Remus {

ConversionController::ConversionController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
{
    connect(this, &ConversionController::libraryChanged,
            m_appController, &AppController::refreshSelectedFile);
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

    const QString rawFormat = format.trimmed().isEmpty() ? m_targetFormat : format.trimmed().toUpper();
    const FileRecord file = m_appController->database()->getFileById(fileId);
    if (file.id <= 0) {
        setLastMessage(QStringLiteral("The selected file no longer exists in the database."));
        return;
    }

    std::unique_ptr<QTemporaryDir> tmpDir;
    const QString romPath = extractIfArchive(file.currentPath, tmpDir);

    // For AUTO, resolve the target format from the (possibly extracted) ROM extension
    const QString detectedExt = QFileInfo(romPath).suffix().toLower();
    const QString normalizedFormat = (rawFormat == QStringLiteral("AUTO"))
        ? resolveAutoFormat(detectedExt)
        : rawFormat;
    if (normalizedFormat.isEmpty()) {
        setLastMessage(QStringLiteral("No conversion format determined for \"%1\" — skipped.").arg(file.filename));
        return;
    }

    m_converting = true;
    m_progress = 0;
    m_progressMessage = QStringLiteral("Converting \"%1\" to %2\u2026").arg(QFileInfo(romPath).fileName(), normalizedFormat);
    emit convertingChanged();
    emit progressChanged();
    emit progressMessageChanged();

    ConversionResult result;
    if (normalizedFormat == QStringLiteral("CHD")) {
        result = m_conversionService.convertToCHD(romPath, CHDCodec::Auto, outputPath, [this](int percent, const QString &) {
            m_progress = percent;
            emit progressChanged();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        });
    } else if (normalizedFormat == QStringLiteral("RVZ")) {
        result = m_conversionService.convertToRVZ(romPath, RVZCompression::Auto, outputPath, [this](int percent, const QString &) {
            m_progress = percent;
            emit progressChanged();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        });
    } else if (normalizedFormat == QStringLiteral("CSO")) {
        result = m_conversionService.convertToCSO(romPath, outputPath, [this](int percent, const QString &) {
            m_progress = percent;
            emit progressChanged();
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        });
    } else if (normalizedFormat == QStringLiteral("WBFS")) {
        result = m_wbfsConverter.convertIsoToWbfs(romPath, outputPath);
        m_progress = 100;
        emit progressChanged();
    } else if (normalizedFormat == QStringLiteral("PBP")) {
        result = m_pbpExporter.exportToPBP(romPath, outputPath);
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

    // Move converted file out of temp dir to the original archive's directory
    // before tmpDir goes out of scope and deletes it.
    QString finalOutputPath = result.outputPath;
    if (tmpDir && !result.outputPath.isEmpty()) {
        const QFileInfo outInfo(result.outputPath);
        const QString destDir = QFileInfo(file.currentPath).absolutePath();
        const QString destPath = destDir + QDir::separator() + outInfo.fileName();
        if (QFile::rename(result.outputPath, destPath))
            finalOutputPath = destPath;
    }

    m_compressionRatio = result.compressionRatio;
    m_lastOutputPath = finalOutputPath;
    {
        QFileInfo outInfo(finalOutputPath);
        FileRecord updated = file;
        updated.currentPath = finalOutputPath;
        updated.filename = outInfo.fileName();
        updated.extension = outInfo.suffix().toLower();
        updated.fileSize = outInfo.size();
        updated.isCompressed = false;
        updated.archivePath.clear();
        updated.archiveInternalPath.clear();
        // Keep hashes — they represent the game content and preserve the
        // confirmed match; re-hashing is user-initiated if needed.
        m_appController->database()->updateFileStorageState(updated);

        QSqlQuery upd(m_appController->database()->database());
        upd.prepare(QStringLiteral("UPDATE files SET is_converted = 1 WHERE id = ?"));
        upd.addBindValue(file.id);
        upd.exec();
    }
    m_progressMessage = QStringLiteral("Created %1").arg(QFileInfo(finalOutputPath).fileName());
    emit progressMessageChanged();
    setLastMessage(QStringLiteral("Created %1").arg(QFileInfo(finalOutputPath).fileName()));
    emit conversionFinished();
    emit libraryChanged();
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

        m_progressMessage = QStringLiteral("Converting %1 / %2: \"%3\"\u2026")
                                .arg(i + 1).arg(total).arg(QFileInfo(file.currentPath).fileName());
        emit progressMessageChanged();

        std::unique_ptr<QTemporaryDir> tmpDir;
        const QString romPath = extractIfArchive(file.currentPath, tmpDir);

        // For AUTO, resolve from the (possibly extracted) ROM extension
        const QString detectedExt = QFileInfo(romPath).suffix().toLower();
        const QString normalizedFormat = (normalizedInput == QStringLiteral("AUTO"))
            ? resolveAutoFormat(detectedExt)
            : normalizedInput;

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
            result = m_conversionService.convertToCHD(romPath, CHDCodec::Auto, outputPath, progress);
        } else if (normalizedFormat == QStringLiteral("RVZ")) {
            result = m_conversionService.convertToRVZ(romPath, RVZCompression::Auto, outputPath, progress);
        } else if (normalizedFormat == QStringLiteral("CSO")) {
            result = m_conversionService.convertToCSO(romPath, outputPath, progress);
        } else if (normalizedFormat == QStringLiteral("WBFS")) {
            result = m_wbfsConverter.convertIsoToWbfs(romPath, outputPath);
        } else if (normalizedFormat == QStringLiteral("PBP")) {
            result = m_pbpExporter.exportToPBP(romPath, outputPath);
        } else {
            ++skipped;
            continue;
        }

        m_progress = (i + 1) * 100 / total;
        emit progressChanged();

        if (result.success) {
            ++converted;
            // Move output out of temp dir before tmpDir is destroyed
            QString finalOutputPath = result.outputPath;
            if (tmpDir && !result.outputPath.isEmpty()) {
                const QFileInfo outInfo(result.outputPath);
                const QString destDir = QFileInfo(file.currentPath).absolutePath();
                const QString destPath = destDir + QDir::separator() + outInfo.fileName();
                if (QFile::rename(result.outputPath, destPath))
                    finalOutputPath = destPath;
            }
            QFileInfo outInfo(finalOutputPath);
            FileRecord updated = file;
            updated.currentPath = finalOutputPath;
            updated.filename = outInfo.fileName();
            updated.extension = outInfo.suffix().toLower();
            updated.fileSize = outInfo.size();
            updated.isCompressed = false;
            updated.archivePath.clear();
            updated.archiveInternalPath.clear();
            // Keep hashes to preserve confirmed match
            m_appController->database()->updateFileStorageState(updated);
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

void ConversionController::setLastMessage(const QString &message)
{
    if (m_lastMessage == message) {
        return;
    }

    m_lastMessage = message;
    emit lastMessageChanged();
}

} // namespace Remus