#include "chd_converter.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>

namespace Remus {

CHDConverter::CHDConverter(QObject *parent)
    : QObject(parent)
    , m_chdmanPath("chdman")  // Use PATH by default
{
}

bool CHDConverter::isChdmanAvailable() const
{
    auto result = const_cast<CHDConverter*>(this)
                      ->runProcess(m_chdmanPath, QStringList() << "--help", 5000);
    return result.started &&
           (result.exitCode == 0 || result.exitStatus == QProcess::NormalExit);
}

QString CHDConverter::getChdmanVersion() const
{
    auto result = const_cast<CHDConverter*>(this)
                      ->runProcess(m_chdmanPath, QStringList() << "--help", 5000);
    QString output = result.stdOutput;
    
    // Parse version from output (typically first line)
    QStringList lines = output.split('\n');
    if (!lines.isEmpty()) {
        return lines.first().trimmed();
    }
    
    return QString();
}

void CHDConverter::setChdmanPath(const QString &path)
{
    m_chdmanPath = path;
}

void CHDConverter::setNumProcessors(int numProcessors)
{
    m_numProcessors = numProcessors;
}

void CHDConverter::setCodec(CHDCodec codec)
{
    m_codec = codec;
}

CHDConversionResult CHDConverter::convertCueToCHD(const QString &cuePath,
                                                   const QString &outputPath)
{
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(cuePath) : outputPath;
    
    QStringList args;
    args << "createcd" << "-i" << cuePath << "-o" << output;
    
    // Add compression codec if specified
    QString codec = getCodecString();
    if (!codec.isEmpty()) {
        args << "-c" << codec;
    }
    
    // Add processor count if specified
    if (m_numProcessors > 0) {
        args << "-np" << QString::number(m_numProcessors);
    }
    
    return runChdman(args, cuePath, output);
}

CHDConversionResult CHDConverter::convertIsoToCHD(const QString &isoPath,
                                                   const QString &outputPath)
{
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(isoPath) : outputPath;
    
    QStringList args;
    args << "createcd" << "-i" << isoPath << "-o" << output;
    
    QString codec = getCodecString();
    if (!codec.isEmpty()) {
        args << "-c" << codec;
    }
    
    if (m_numProcessors > 0) {
        args << "-np" << QString::number(m_numProcessors);
    }
    
    return runChdman(args, isoPath, output);
}

CHDConversionResult CHDConverter::convertGdiToCHD(const QString &gdiPath,
                                                   const QString &outputPath)
{
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(gdiPath) : outputPath;
    
    QStringList args;
    args << "createcd" << "-i" << gdiPath << "-o" << output;
    
    QString codec = getCodecString();
    if (!codec.isEmpty()) {
        args << "-c" << codec;
    }
    
    if (m_numProcessors > 0) {
        args << "-np" << QString::number(m_numProcessors);
    }
    
    return runChdman(args, gdiPath, output);
}

CHDConversionResult CHDConverter::extractCHDToCue(const QString &chdPath,
                                                   const QString &outputPath)
{
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(chdPath, "cue") : outputPath;
    
    QStringList args;
    args << "extractcd" << "-i" << chdPath << "-o" << output;
    
    return runChdman(args, chdPath, output);
}

CHDVerifyResult CHDConverter::verifyCHD(const QString &chdPath)
{
    CHDVerifyResult result;
    result.path = chdPath;

    ProcessResult processResult = runProcess(m_chdmanPath,
                                             QStringList() << "verify" << "-i" << chdPath,
                                             300000);

    result.valid = (processResult.exitCode == 0);
    result.details = processResult.stdOutput;

    if (!result.valid) {
        result.error = processResult.stdError.isEmpty() ?
                        "Verification failed" :
                        processResult.stdError;
    }
    
    return result;
}

CHDInfo CHDConverter::getCHDInfo(const QString &chdPath)
{
    CHDInfo info;
    info.path = chdPath;
    info.physicalSize = getFileSize(chdPath);

    ProcessResult processResult = runProcess(m_chdmanPath,
                                             QStringList() << "info" << "-i" << chdPath,
                                             30000);

    QString output = processResult.stdOutput;
    
    // Parse chdman info output
    QRegularExpression versionRe("CHD version:\\s+(\\d+)");
    QRegularExpression logicalRe("Logical size:\\s+(\\d+)");
    QRegularExpression sha1Re("SHA1:\\s+([a-fA-F0-9]+)");
    QRegularExpression compressionRe("Compression:\\s+(\\S+)");
    
    QRegularExpressionMatch match;
    
    match = versionRe.match(output);
    if (match.hasMatch()) {
        info.version = match.captured(1).toInt();
    }
    
    match = logicalRe.match(output);
    if (match.hasMatch()) {
        info.logicalSize = match.captured(1).toLongLong();
    }
    
    match = sha1Re.match(output);
    if (match.hasMatch()) {
        info.sha1 = match.captured(1);
    }
    
    match = compressionRe.match(output);
    if (match.hasMatch()) {
        info.compression = match.captured(1);
    }
    
    return info;
}

QList<CHDConversionResult> CHDConverter::batchConvert(const QStringList &inputPaths,
                                                       const QString &outputDir)
{
    QList<CHDConversionResult> results;
    m_cancelled = false;
    
    int total = inputPaths.size();
    int completed = 0;
    
    for (const QString &inputPath : inputPaths) {
        if (m_cancelled) {
            emit conversionCancelled();
            break;
        }
        
        QString outputPath;
        if (!outputDir.isEmpty()) {
            QFileInfo inputInfo(inputPath);
            outputPath = QDir(outputDir).filePath(inputInfo.completeBaseName() + ".chd");
        }
        
        QFileInfo info(inputPath);
        QString ext = info.suffix().toLower();
        
        CHDConversionResult result;
        
        if (ext == "cue") {
            result = convertCueToCHD(inputPath, outputPath);
        } else if (ext == "iso") {
            result = convertIsoToCHD(inputPath, outputPath);
        } else if (ext == "gdi") {
            result = convertGdiToCHD(inputPath, outputPath);
        } else {
            result.success = false;
            result.inputPath = inputPath;
            result.error = QString("Unsupported format: %1").arg(ext);
        }
        
        results.append(result);
        completed++;
        
        emit batchProgress(completed, total);
    }
    
    return results;
}

void CHDConverter::cancel()
{
    m_cancelled = true;
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->terminate();
        m_process->waitForFinished(3000);
        if (m_process->state() == QProcess::Running) {
            m_process->kill();
        }
    }
}

bool CHDConverter::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

CHDConversionResult CHDConverter::runChdman(const QStringList &args,
                                             const QString &inputPath,
                                             const QString &outputPath)
{
    CHDConversionResult result;
    result.inputPath = inputPath;
    result.outputPath = outputPath;
    result.inputSize = getFileSize(inputPath);
    
    // For BIN/CUE, add BIN file sizes too
    if (inputPath.endsWith(".cue", Qt::CaseInsensitive)) {
        QFileInfo cueInfo(inputPath);
        QDir dir = cueInfo.absoluteDir();
        QString baseName = cueInfo.completeBaseName();
        
        // Look for matching .bin files
        QStringList binFilters;
        binFilters << baseName + ".bin" << baseName + " (Track*).bin";
        QFileInfoList binFiles = dir.entryInfoList(binFilters, QDir::Files);
        
        for (const QFileInfo &binInfo : binFiles) {
            result.inputSize += binInfo.size();
        }
    }
    
    emit conversionStarted(inputPath, outputPath);
    
    qInfo() << "Running chdman:" << m_chdmanPath << args.join(" ");
    
    ProcessResult processResult = runProcessTracked(m_chdmanPath, args, 1800000);
    if (!processResult.started) {
        result.success = false;
        result.error = "Failed to start chdman. Is it installed?";
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
            result.compressionRatio = static_cast<double>(result.outputSize) / 
                                       static_cast<double>(result.inputSize);
        }
        
        qInfo() << "CHD conversion successful:" << inputPath << "->" << outputPath;
        qInfo() << "Compression ratio:" << QString::number(result.compressionRatio * 100, 'f', 1) << "%";
    } else {
        result.success = false;
        result.error = result.stdError.isEmpty() ?
                       QString("chdman exited with code %1").arg(result.exitCode) :
                       result.stdError;
        qWarning() << "CHD conversion failed:" << result.error;
    }
    
    emit conversionCompleted(result);
    
    return result;
}

CHDConverter::ProcessResult CHDConverter::runProcess(const QString &program,
                                                     const QStringList &args,
                                                     int timeoutMs)
{
    ProcessResult result;
    QProcess process;

    process.start(program, args);
    result.started = process.waitForStarted(timeoutMs);
    if (!result.started) {
        result.exitCode = -1;
        return result;
    }

    result.finished = process.waitForFinished(timeoutMs);
    result.exitCode = process.exitCode();
    result.exitStatus = process.exitStatus();
    result.stdOutput = QString::fromUtf8(process.readAllStandardOutput());
    result.stdError = QString::fromUtf8(process.readAllStandardError());
    return result;
}

CHDConverter::ProcessResult CHDConverter::runProcessTracked(const QString &program,
                                                            const QStringList &args,
                                                            int timeoutMs)
{
    ProcessResult result;
    QProcess process;
    m_process = &process;

    process.start(program, args);
    result.started = process.waitForStarted(10000);
    if (!result.started) {
        m_process = nullptr;
        result.exitCode = -1;
        return result;
    }

    result.finished = process.waitForFinished(timeoutMs);
    result.exitCode = process.exitCode();
    result.exitStatus = process.exitStatus();
    result.stdOutput = QString::fromUtf8(process.readAllStandardOutput());
    result.stdError = QString::fromUtf8(process.readAllStandardError());
    m_process = nullptr;
    return result;
}

QString CHDConverter::getDefaultOutputPath(const QString &inputPath, const QString &targetExt)
{
    QFileInfo info(inputPath);
    return info.absoluteDir().filePath(info.completeBaseName() + "." + targetExt);
}

QString CHDConverter::getCodecString() const
{
    switch (m_codec) {
        case CHDCodec::LZMA:     return "lzma";
        case CHDCodec::ZLIB:     return "zlib";
        case CHDCodec::FLAC:     return "flac";
        case CHDCodec::Huffman:  return "huff";
        case CHDCodec::Auto:
        default:                 return QString();
    }
}

qint64 CHDConverter::getFileSize(const QString &path) const
{
    QFileInfo info(path);
    return info.exists() ? info.size() : 0;
}

} // namespace Remus
