#include "disc_set_utils.h"

#include "disc_set_key.h"
#include "disc_title_parser.h"

#include <QFileInfo>

namespace Remus {

namespace {

    QString basenameOrEmpty(const QString &path) {
        if (path.isEmpty())
            return QString();
        return QFileInfo(path).fileName();
    }

} // namespace

QString DiscSetUtils::labelPath(const QString &currentPath, const QString &archivePath,
    const QString &archiveInternalPath, const QString &filename) {
    const QString currentBase = basenameOrEmpty(currentPath);
    const QString archiveBase = basenameOrEmpty(archivePath);
    const QString internalBase = basenameOrEmpty(archiveInternalPath);
    const QString fileBase = basenameOrEmpty(filename);

    if (isMultiDisc(currentBase))
        return currentBase;
    if (isMultiDisc(fileBase))
        return fileBase;
    if (isMultiDisc(internalBase))
        return internalBase;
    if (isMultiDisc(archiveBase))
        return archiveBase;

    if (!currentBase.isEmpty())
        return currentBase;
    if (!fileBase.isEmpty())
        return fileBase;
    return filename;
}

bool DiscSetUtils::isMultiDisc(const QString &labelPath) {
    return DiscTitleParser::isMultiDisc(labelPath);
}

QString DiscSetUtils::extractBaseTitle(const QString &labelPath) {
    return DiscTitleParser::extractBaseTitle(labelPath);
}

int DiscSetUtils::extractDiscNumber(const QString &labelPath) {
    return DiscTitleParser::extractDiscNumber(labelPath);
}

QString DiscSetUtils::groupKey(const QString &labelPath, const QString &systemName) {
    return DiscSetKey::legacyLibraryGroupKey(labelPath, systemName);
}

QString DiscSetUtils::gameDiscSetKey(int gameId, int systemId) {
    return QStringLiteral("game:%1|%2").arg(gameId).arg(systemId);
}

QString DiscSetUtils::discRowLabel(const QString &labelPath, int discNumber) {
    if (discNumber > 0)
        return QStringLiteral("Disc %1").arg(discNumber);
    if (isMultiDisc(labelPath))
        return extractBaseTitle(labelPath);
    return labelPath;
}

QString DiscSetUtils::sanitizeFolderComponent(const QString &name) {
    QString cleaned = name.trimmed();
    cleaned.replace(QLatin1Char('/'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char('\\'), QLatin1Char('_'));
    cleaned.replace(QLatin1Char(':'), QLatin1Char('_'));
    while (cleaned.endsWith(QLatin1Char('.')) || cleaned.endsWith(QLatin1Char(' ')))
        cleaned.chop(1);
    return cleaned;
}

void DiscSetUtils::applyScanDiscMetadata(FileRecord &record, const QString &systemName) {
    const QString label
        = labelPath(record.currentPath, record.archivePath, record.archiveInternalPath, record.filename);

    record.discNumber = 0;
    record.discSetKey.clear();

    if (!isMultiDisc(label))
        return;

    const DiscTitleInfo parsed = DiscTitleParser::parseTitle(label);
    record.discNumber = parsed.discNumber;
    if (record.baseTitle.isEmpty())
        record.baseTitle = parsed.baseTitle;
    record.discSetKey = groupKey(label, systemName);
}

} // namespace Remus
