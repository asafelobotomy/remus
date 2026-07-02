#include "disc_converter.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QProcess>

namespace Remus {

DiscConverter::DiscConverter(QObject *parent)
    : ExternalToolRunner(parent) { }

void DiscConverter::cancel() {
    ExternalToolRunner::cancel();
    emit conversionCancelled();
}

ConversionResult DiscConverter::runToolConversion(const QString &toolPath, const QStringList &args,
    const QString &toolDisplayName, const QString &inputPath, const QString &outputPath, qint64 inputSize,
    int timeoutMs) {
    ConversionResult result;
    result.inputPath = inputPath;
    result.outputPath = outputPath;
    result.inputSize = (inputSize >= 0) ? inputSize : getFileSize(inputPath);

    emit conversionStarted(inputPath, outputPath);

    qInfo() << "Running" << toolDisplayName << ":" << toolPath << args.join(" ");

    QProcess process;
    m_process = &process;
    m_cancelled = false;
    process.start(toolPath, args);

    ProcessResult processResult;
    processResult.started = process.waitForStarted(10000);
    if (!processResult.started) {
        m_process = nullptr;
        processResult.exitCode = -1;
        if (processResult.stdError.isEmpty()) {
            processResult.stdError = QStringLiteral("Failed to start process: %1").arg(toolPath);
        }
    } else {
        QElapsedTimer elapsed;
        elapsed.start();
        int lastHeartbeatSec = 0;
        while (process.state() == QProcess::Running) {
            if (!process.waitForFinished(1000)) {
                if (m_cancelled) {
                    process.kill();
                    process.waitForFinished(3000);
                    break;
                }
                const int sec = static_cast<int>(elapsed.elapsed() / 1000);
                if (sec >= lastHeartbeatSec + 5) {
                    lastHeartbeatSec = sec;
                    emit conversionProgress(0, QStringLiteral("%1 running… %2s elapsed").arg(toolDisplayName).arg(sec));
                }
                continue;
            }
            break;
        }
        processResult.finished = process.state() == QProcess::NotRunning;
        if (!processResult.finished && !m_cancelled) {
            process.kill();
            process.waitForFinished(3000);
            if (processResult.stdError.isEmpty()) {
                processResult.stdError
                    = QStringLiteral("Process timed out after %1 ms: %2").arg(timeoutMs).arg(toolPath);
            }
        }
        processResult.exitCode = process.exitCode();
        processResult.exitStatus = process.exitStatus();
        processResult.stdOutput = QString::fromUtf8(process.readAllStandardOutput());
        processResult.stdError = QString::fromUtf8(process.readAllStandardError());
        m_process = nullptr;
    }
    if (!processResult.started) {
        result.success = false;
        result.error = QString("Failed to start %1. Is it installed?").arg(toolDisplayName);
        result.exitCode = -1;
        emit errorOccurred(result.error);
        return result;
    }

    result.exitCode = processResult.exitCode;
    result.stdOutput = processResult.stdOutput;
    result.stdError = processResult.stdError;

    if (result.exitCode == 0 && QFile::exists(outputPath)) {
        result.success = true;
        result.outputSize = getFileSize(outputPath);

        if (result.inputSize > 0) {
            result.compressionRatio = static_cast<double>(result.outputSize) / static_cast<double>(result.inputSize);
        }

        qInfo() << toolDisplayName << "conversion successful:" << inputPath << "->" << outputPath;
        qInfo() << "Compression ratio:" << QString::number(result.compressionRatio * 100, 'f', 1) << "%";
    } else {
        result.success = false;
        result.error = result.stdError.isEmpty()
            ? QString("%1 exited with code %2").arg(toolDisplayName).arg(result.exitCode)
            : result.stdError;
        qWarning() << toolDisplayName << "conversion failed:" << result.error;
    }

    emit conversionCompleted(result);
    return result;
}

qint64 DiscConverter::getFileSize(const QString &path) {
    QFileInfo info(path);
    return info.exists() ? info.size() : 0;
}

QString DiscConverter::getDefaultOutputPath(const QString &inputPath, const QString &targetExt) {
    QFileInfo info(inputPath);
    QString ext = targetExt.startsWith('.') ? targetExt.mid(1) : targetExt;
    return info.absoluteDir().filePath(info.completeBaseName() + QStringLiteral(".") + ext);
}

} // namespace Remus
