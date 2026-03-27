#include "chd_converter.h"
#include "constants/files.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>

namespace Remus {

CHDConverter::CHDConverter(QObject *parent)
    : DiscConverter(parent)
    , m_chdmanPath("chdman")
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

QStringList CHDConverter::buildCreateCdArgs(const QString &inputPath, const QString &outputPath)
{
    QStringList args;
    args << "createcd" << "-i" << inputPath << "-o" << outputPath;

    QString codec = getCodecString();
    if (!codec.isEmpty()) {
        args << "-c" << codec;
    }
    if (m_numProcessors > 0) {
        args << "-np" << QString::number(m_numProcessors);
    }
    return args;
}

ConversionResult CHDConverter::convertCueToCHD(const QString &cuePath,
                                                const QString &outputPath)
{
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(cuePath, "chd") : outputPath;
    return runChdman(buildCreateCdArgs(cuePath, output), cuePath, output);
}

ConversionResult CHDConverter::convertIsoToCHD(const QString &isoPath,
                                                const QString &outputPath)
{
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(isoPath, "chd") : outputPath;
    return runChdman(buildCreateCdArgs(isoPath, output), isoPath, output);
}

ConversionResult CHDConverter::convertGdiToCHD(const QString &gdiPath,
                                                const QString &outputPath)
{
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(gdiPath, "chd") : outputPath;
    return runChdman(buildCreateCdArgs(gdiPath, output), gdiPath, output);
}

ConversionResult CHDConverter::extractCHDToCue(const QString &chdPath,
                                                const QString &outputPath)
{
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(chdPath, "cue") : outputPath;
    QStringList args;
    args << "extractcd" << "-i" << chdPath << "-o" << output;
    return runToolConversion(m_chdmanPath, args, "chdman", chdPath, output);
}

VerifyResult CHDConverter::verifyCHD(const QString &chdPath)
{
    VerifyResult result;
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

    if (!processResult.started || processResult.exitCode != 0) {
        return info;
    }

    const QString output = processResult.stdOutput;

    auto parseInt64 = [](QString value) {
        value.remove(',');
        return value.toLongLong();
    };

    // Parse chdman info output. Recent chdman versions use labels such as
    // "File Version" and "CHD size" rather than the older "CHD version"
    // and "Physical size" wording.
    const QRegularExpression versionRe(R"((?:CHD version|File Version):\s*(\d+))");
    const QRegularExpression logicalRe(R"(Logical size:\s*([\d,]+))");
    const QRegularExpression physicalRe(R"((?:CHD size|Physical size):\s*([\d,]+))");
    const QRegularExpression sha1Re(R"(^SHA1:\s*([a-fA-F0-9]+))", QRegularExpression::MultilineOption);
    const QRegularExpression compressionRe(R"(^Compression:\s*(.+)$)", QRegularExpression::MultilineOption);

    QRegularExpressionMatch match = versionRe.match(output);
    if (match.hasMatch()) {
        info.version = match.captured(1).toInt();
    }

    match = logicalRe.match(output);
    if (match.hasMatch()) {
        info.logicalSize = parseInt64(match.captured(1));
    }

    match = physicalRe.match(output);
    if (match.hasMatch()) {
        info.physicalSize = parseInt64(match.captured(1));
    }

    match = sha1Re.match(output);
    if (match.hasMatch()) {
        info.sha1 = match.captured(1);
    }

    match = compressionRe.match(output);
    if (match.hasMatch()) {
        info.compression = match.captured(1).trimmed();
    }
    
    return info;
}

QList<ConversionResult> CHDConverter::batchConvert(const QStringList &inputPaths,
                                                    const QString &outputDir)
{
    QList<ConversionResult> results;
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
            outputPath = QDir(outputDir).filePath(inputInfo.completeBaseName() + Constants::Files::CHD);
        }
        
        QFileInfo info(inputPath);
        QString ext = info.suffix().toLower();
        
        ConversionResult result;
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

ConversionResult CHDConverter::runChdman(const QStringList &args,
                                          const QString &inputPath,
                                          const QString &outputPath)
{
    qint64 inputSize = getFileSize(inputPath);

    // For BIN/CUE, add BIN file sizes
    if (inputPath.endsWith(Constants::Files::CUE, Qt::CaseInsensitive)) {
        QFileInfo cueInfo(inputPath);
        QDir dir = cueInfo.absoluteDir();
        QString baseName = cueInfo.completeBaseName();

        QStringList binFilters;
        binFilters << baseName + Constants::Files::BIN
                   << baseName + QStringLiteral(" (Track*)") + Constants::Files::BIN;
        for (const QFileInfo &binInfo : dir.entryInfoList(binFilters, QDir::Files)) {
            inputSize += binInfo.size();
        }
    }

    return runToolConversion(m_chdmanPath, args, "chdman", inputPath, outputPath, inputSize);
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

} // namespace Remus
