#include "wbfs_converter.h"
#include "constants/files.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>

namespace Remus {

WBFSConverter::WBFSConverter(QObject *parent)
    : DiscConverter(parent)
    , m_witPath("wit")
{
}

bool WBFSConverter::isWitAvailable() const
{
    auto result = const_cast<WBFSConverter*>(this)
                      ->runProcess(m_witPath, QStringList() << "--version", 5000);
    return result.started;
}

QString WBFSConverter::getWitVersion() const
{
    auto result = const_cast<WBFSConverter*>(this)
                      ->runProcess(m_witPath, QStringList() << "--version", 5000);
    QString output = result.stdOutput.isEmpty() ? result.stdError : result.stdOutput;
    QStringList lines = output.split('\n');
    return lines.isEmpty() ? QString() : lines.first().trimmed();
}

void WBFSConverter::setWitPath(const QString &path)
{
    m_witPath = path;
}

ConversionResult WBFSConverter::convertIsoToWbfs(const QString &isoPath,
                                                   const QString &outputPath)
{
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(isoPath, "wbfs") : outputPath;

    // wit copy <source> --dest <dest>
    QStringList args;
    args << "copy"
         << isoPath
         << "--dest" << output;

    return runToolConversion(m_witPath, args, "wit", isoPath, output);
}

ConversionResult WBFSConverter::extractWbfsToIso(const QString &wbfsPath,
                                                   const QString &outputPath)
{
    QString output = outputPath.isEmpty() ? getDefaultOutputPath(wbfsPath, "iso") : outputPath;

    // wit copy <source.wbfs> --dest <dest.iso>
    QStringList args;
    args << "copy"
         << wbfsPath
         << "--dest" << output;

    return runToolConversion(m_witPath, args, "wit", wbfsPath, output);
}

} // namespace Remus
