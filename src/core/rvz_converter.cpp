#include "rvz_converter.h"
#include "constants/files.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>

namespace Remus {

RVZConverter::RVZConverter(QObject *parent)
    : DiscConverter(parent)
    , m_dolphinToolPath("dolphin-tool") { }

bool RVZConverter::isDolphinToolAvailable() const {
    auto result = const_cast<RVZConverter *>(this)->runProcess(m_dolphinToolPath, QStringList() << "--help", 5000);
    return result.started;
}

QString RVZConverter::getDolphinToolVersion() const {
    auto result = const_cast<RVZConverter *>(this)->runProcess(m_dolphinToolPath, QStringList() << "--help", 5000);
    // dolphin-tool prints version info in help output
    QStringList lines = result.stdOutput.split('\n');
    for (const QString &line : lines) {
        if (line.contains("dolphin", Qt::CaseInsensitive) && line.contains("tool", Qt::CaseInsensitive)) {
            return line.trimmed();
        }
    }
    return lines.isEmpty() ? QString() : lines.first().trimmed();
}

void RVZConverter::setDolphinToolPath(const QString &path) {
    m_dolphinToolPath = path;
}

void RVZConverter::setCompression(RVZCompression compression) {
    m_compression = compression;
}

void RVZConverter::setCompressionLevel(int level) {
    m_compressionLevel = level;
}

ConversionResult RVZConverter::convertIsoToRVZ(const QString &isoPath, const QString &outputPath) {
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(isoPath, "rvz") : outputPath;

    QStringList args;
    args << "convert"
         << "--format=rvz"
         << "--block_size=131072"
         << "--input" << isoPath << "--output" << output;

    QString compression = getCompressionString();
    if (!compression.isEmpty()) {
        args << ("--compression=" + compression);
        args << ("--compression_level=" + QString::number(m_compressionLevel));
    }

    return runToolConversion(m_dolphinToolPath, args, "dolphin-tool", isoPath, output);
}

ConversionResult RVZConverter::extractRVZToIso(const QString &rvzPath, const QString &outputPath) {
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(rvzPath, "iso") : outputPath;

    QStringList args;
    args << "convert"
         << "--format=iso"
         << "--input" << rvzPath << "--output" << output;

    return runToolConversion(m_dolphinToolPath, args, "dolphin-tool", rvzPath, output);
}

VerifyResult RVZConverter::verifyRVZ(const QString &rvzPath) {
    VerifyResult result;
    result.path = rvzPath;

    ProcessResult processResult
        = runProcess(m_dolphinToolPath, QStringList() << "verify" << "--input" << rvzPath, 300000);

    result.valid = (processResult.exitCode == 0);
    result.details = processResult.stdOutput;

    if (!result.valid) {
        result.error = processResult.stdError.isEmpty() ? "Verification failed" : processResult.stdError;
    }

    return result;
}

QString RVZConverter::discContentSha1(const QString &discPath) {
    if (discPath.trimmed().isEmpty())
        return QString();

    ProcessResult processResult = runProcess(m_dolphinToolPath,
        QStringList() << QStringLiteral("verify") << QStringLiteral("-a") << QStringLiteral("sha1")
                      << QStringLiteral("--input") << discPath,
        600000);

    if (processResult.exitCode != 0)
        return QString();

    const QString digest = processResult.stdOutput.trimmed().toLower();
    static const QRegularExpression hexRe(QStringLiteral("^[0-9a-f]{40}$"));
    if (hexRe.match(digest).hasMatch())
        return digest;

    for (const QString &line : processResult.stdOutput.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed().toLower();
        if (trimmed.startsWith(QStringLiteral("sha1:"))) {
            const QString value = trimmed.mid(5).trimmed();
            if (hexRe.match(value).hasMatch())
                return value;
        }
        if (hexRe.match(trimmed).hasMatch())
            return trimmed;
    }

    return QString();
}

QList<ConversionResult> RVZConverter::batchConvert(const QStringList &inputPaths, const QString &outputDir) {
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
            outputPath = QDir(outputDir).filePath(inputInfo.completeBaseName() + ".rvz");
        }

        ConversionResult result = convertIsoToRVZ(inputPath, outputPath);
        results.append(result);
        completed++;

        emit batchProgress(completed, total);
    }

    return results;
}

QString RVZConverter::getCompressionString() const {
    switch (m_compression) {
    case RVZCompression::Zstd:
        return "zstd";
    case RVZCompression::Bzip2:
        return "bzip2";
    case RVZCompression::LZMA:
        return "lzma";
    case RVZCompression::LZMA2:
        return "lzma2";
    case RVZCompression::None:
        return "none";
    case RVZCompression::Auto:
    default:
        return "zstd";
    }
}

} // namespace Remus
