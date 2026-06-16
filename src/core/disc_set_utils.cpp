#include "disc_set_utils.h"

#include <QFileInfo>
#include <QRegularExpression>

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
    static const QRegularExpression re(
        QStringLiteral("\\b(Disc|CD|Disk)\\s*\\d+"), QRegularExpression::CaseInsensitiveOption);
    return re.match(labelPath).hasMatch();
}

QString DiscSetUtils::extractBaseTitle(const QString &labelPath) {
    QString baseTitle = labelPath;
    baseTitle = QFileInfo(baseTitle).completeBaseName();

    static const QRegularExpression discPattern(
        QStringLiteral("\\s*\\(?\\s*(Disc|CD|Disk)\\s*\\d+.*?\\)?\\s*"),
        QRegularExpression::CaseInsensitiveOption);
    baseTitle.remove(discPattern);

    baseTitle = baseTitle.trimmed();
    baseTitle.replace(QRegularExpression(QStringLiteral("\\s{2,}")), QStringLiteral(" "));
    baseTitle.replace(QRegularExpression(QStringLiteral("\\(\\s*\\)")), QString());

    return baseTitle;
}

int DiscSetUtils::extractDiscNumber(const QString &labelPath) {
    static const QRegularExpression re(
        QStringLiteral("\\b(Disc|CD|Disk)\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(labelPath);
    if (match.hasMatch())
        return match.captured(2).toInt();
    return 0;
}

QString DiscSetUtils::groupKey(const QString &labelPath, const QString &systemName) {
    return extractBaseTitle(labelPath) + QChar('|') + systemName.trimmed();
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

void DiscSetUtils::applyScanDiscMetadata(FileRecord &record, const QString &systemName) {
    const QString label = labelPath(
        record.currentPath, record.archivePath, record.archiveInternalPath, record.filename);

    record.discNumber = 0;
    record.discSetKey.clear();

    if (!isMultiDisc(label))
        return;

    record.discNumber = extractDiscNumber(label);
    if (record.baseTitle.isEmpty())
        record.baseTitle = extractBaseTitle(label);
    record.discSetKey = groupKey(label, systemName);
}

} // namespace Remus
