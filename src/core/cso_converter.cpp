#include "cso_converter.h"
#include "constants/files.h"
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QDebug>

namespace Remus {

CSOConverter::CSOConverter(QObject *parent)
    : DiscConverter(parent)
    , m_maxcsoPath("maxcso") { }

bool CSOConverter::isMaxcsoAvailable() const {
    auto result = const_cast<CSOConverter *>(this)->runProcess(m_maxcsoPath, QStringList() << "--help", 5000);
    return result.started;
}

QString CSOConverter::getMaxcsoVersion() const {
    auto result = const_cast<CSOConverter *>(this)->runProcess(m_maxcsoPath, QStringList() << "--version", 5000);
    // maxcso may print version to stdout or stderr
    QString output = result.stdOutput.isEmpty() ? result.stdError : result.stdOutput;
    QStringList lines = output.split('\n');
    return lines.isEmpty() ? QString() : lines.first().trimmed();
}

void CSOConverter::setMaxcsoPath(const QString &path) {
    m_maxcsoPath = path;
}

ConversionResult CSOConverter::convertIsoToCSO(const QString &isoPath, const QString &outputPath) {
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(isoPath, "cso") : outputPath;

    QStringList args;
    args << isoPath << "-o" << output;

    return runToolConversion(m_maxcsoPath, args, "maxcso", isoPath, output);
}

ConversionResult CSOConverter::extractCSOToIso(const QString &csoPath, const QString &outputPath) {
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(csoPath, "iso") : outputPath;

    QStringList args;
    args << "--decompress" << csoPath << "-o" << output;

    return runToolConversion(m_maxcsoPath, args, "maxcso", csoPath, output);
}

CSOVerifyResult CSOConverter::verifyCSO(const QString &csoPath) {
    CSOVerifyResult result;

    if (!QFileInfo::exists(csoPath)) {
        result.error = QStringLiteral("File not found: %1").arg(csoPath);
        return result;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        result.error = QStringLiteral("Failed to create temporary directory");
        return result;
    }

    QFileInfo info(csoPath);
    const QString isoPath = tempDir.filePath(info.completeBaseName() + QStringLiteral(".iso"));

    const ConversionResult extraction = extractCSOToIso(csoPath, isoPath);
    if (!extraction.success) {
        result.error = extraction.error;
        return result;
    }

    QFileInfo isoInfo(isoPath);
    if (!isoInfo.exists() || isoInfo.size() == 0) {
        result.error = QStringLiteral("Extracted ISO is missing or empty");
        return result;
    }

    result.valid = true;
    result.isoSize = isoInfo.size();
    return result;
}

QList<ConversionResult> CSOConverter::batchConvert(const QStringList &inputPaths, const QString &outputDir) {
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
            outputPath = QDir(outputDir).filePath(inputInfo.completeBaseName() + ".cso");
        }

        QFileInfo info(inputPath);
        const QString ext = QStringLiteral(".") + info.suffix().toLower();

        ConversionResult result;
        if (Constants::Files::containsExtension(Constants::Files::CSO_SOURCE_EXTENSIONS, ext)) {
            result = convertIsoToCSO(inputPath, outputPath);
        } else {
            result.success = false;
            result.inputPath = inputPath;
            result.error = QStringLiteral("Unsupported format: %1").arg(ext);
        }

        results.append(result);
        completed++;

        emit batchProgress(completed, total);
    }

    return results;
}

} // namespace Remus
