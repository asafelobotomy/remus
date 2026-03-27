#include "cso_converter.h"
#include "constants/files.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>

namespace Remus {

CSOConverter::CSOConverter(QObject *parent)
    : DiscConverter(parent)
    , m_maxcsoPath("maxcso")
{
}

bool CSOConverter::isMaxcsoAvailable() const
{
    auto result = const_cast<CSOConverter*>(this)
                      ->runProcess(m_maxcsoPath, QStringList() << "--help", 5000);
    return result.started;
}

QString CSOConverter::getMaxcsoVersion() const
{
    auto result = const_cast<CSOConverter*>(this)
                      ->runProcess(m_maxcsoPath, QStringList() << "--version", 5000);
    // maxcso may print version to stdout or stderr
    QString output = result.stdOutput.isEmpty() ? result.stdError : result.stdOutput;
    QStringList lines = output.split('\n');
    return lines.isEmpty() ? QString() : lines.first().trimmed();
}

void CSOConverter::setMaxcsoPath(const QString &path)
{
    m_maxcsoPath = path;
}

ConversionResult CSOConverter::convertIsoToCSO(const QString &isoPath,
                                                const QString &outputPath)
{
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(isoPath, "cso") : outputPath;

    QStringList args;
    args << isoPath
         << "-o" << output;

    return runToolConversion(m_maxcsoPath, args, "maxcso", isoPath, output);
}

ConversionResult CSOConverter::extractCSOToIso(const QString &csoPath,
                                                const QString &outputPath)
{
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(csoPath, "iso") : outputPath;

    QStringList args;
    args << "--decompress"
         << csoPath
         << "-o" << output;

    return runToolConversion(m_maxcsoPath, args, "maxcso", csoPath, output);
}

QList<ConversionResult> CSOConverter::batchConvert(const QStringList &inputPaths,
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
            outputPath = QDir(outputDir).filePath(inputInfo.completeBaseName() + ".cso");
        }

        ConversionResult result = convertIsoToCSO(inputPath, outputPath);
        results.append(result);
        completed++;

        emit batchProgress(completed, total);
    }

    return results;
}

} // namespace Remus
