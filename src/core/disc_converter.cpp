#include "disc_converter.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>

namespace Remus {

DiscConverter::DiscConverter(QObject *parent)
    : ExternalToolRunner(parent)
{
}

void DiscConverter::cancel()
{
    ExternalToolRunner::cancel();
    emit conversionCancelled();
}

ConversionResult DiscConverter::runToolConversion(const QString &toolPath,
                                                   const QStringList &args,
                                                   const QString &toolDisplayName,
                                                   const QString &inputPath,
                                                   const QString &outputPath,
                                                   qint64 inputSize,
                                                   int timeoutMs)
{
    ConversionResult result;
    result.inputPath = inputPath;
    result.outputPath = outputPath;
    result.inputSize = (inputSize >= 0) ? inputSize : getFileSize(inputPath);

    emit conversionStarted(inputPath, outputPath);

    qInfo() << "Running" << toolDisplayName << ":" << toolPath << args.join(" ");

    ProcessResult processResult = runProcessTracked(toolPath, args, timeoutMs);
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
            result.compressionRatio = static_cast<double>(result.outputSize)
                                       / static_cast<double>(result.inputSize);
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

qint64 DiscConverter::getFileSize(const QString &path)
{
    QFileInfo info(path);
    return info.exists() ? info.size() : 0;
}

QString DiscConverter::getDefaultOutputPath(const QString &inputPath,
                                             const QString &targetExt)
{
    QFileInfo info(inputPath);
    QString ext = targetExt.startsWith('.') ? targetExt.mid(1) : targetExt;
    return info.absoluteDir().filePath(info.completeBaseName() + QStringLiteral(".") + ext);
}

} // namespace Remus
