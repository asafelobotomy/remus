#include "patched_rom_parser.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>

namespace Remus {

namespace {

QString stripTrailingExtension(const QString &nameOrPath)
{
    QString name = QFileInfo(nameOrPath).fileName();
    if (name.isEmpty()) {
        name = nameOrPath.trimmed();
    }

    const int dotPos = name.lastIndexOf('.');
    if (dotPos == -1) {
        return name;
    }

    const QString suffix = name.mid(dotPos + 1);
    static const QRegularExpression extensionRegex("^[A-Za-z0-9]{1,5}$");
    if (!extensionRegex.match(suffix).hasMatch()) {
        return name;
    }

    return name.left(dotPos);
}

QString cleanTag(const QString &tag)
{
    QString cleaned = tag.trimmed();
    if ((cleaned.startsWith('(') && cleaned.endsWith(')')) ||
        (cleaned.startsWith('[') && cleaned.endsWith(']'))) {
        cleaned = cleaned.mid(1, cleaned.size() - 2);
    }
    return cleaned.simplified();
}

bool isOfficialRegionOrRevisionTag(const QString &tag)
{
    static const QRegularExpression pureRegionRegex(
        R"(^((usa|europe|japan|world|asia|australia|brazil|canada|france|germany|italy|spain|korea|china|taiwan|hong kong|english|french|german|spanish|italian|portuguese|dutch|russian)(\s*,\s*(usa|europe|japan|world|asia|australia|brazil|canada|france|germany|italy|spain|korea|china|taiwan|hong kong|english|french|german|spanish|italian|portuguese|dutch|russian))*)$)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression revisionRegex(
        R"(^(rev(ision)?\s*[a-z0-9.]+|v\d+(\.\d+)*|disc\s*\d+|side\s*[a-z0-9]+|alt\s*\d+)$)",
        QRegularExpression::CaseInsensitiveOption);

    return pureRegionRegex.match(tag).hasMatch() || revisionRegex.match(tag).hasMatch();
}

bool looksPrototypeTag(const QString &tag)
{
    static const QRegularExpression regex(
        R"(\b(proto|prototype|beta|demo|sample|preview)\b)",
        QRegularExpression::CaseInsensitiveOption);
    return regex.match(tag).hasMatch();
}

bool looksHomebrewTag(const QString &tag)
{
    static const QRegularExpression regex(
        R"(\b(homebrew|aftermarket|public domain|pd|unlicensed|home brew)\b)",
        QRegularExpression::CaseInsensitiveOption);
    return regex.match(tag).hasMatch();
}

bool looksTranslationTag(const QString &tag)
{
    static const QRegularExpression explicitTranslationRegex(
        R"(\b(tr(\s|$)|translation|translated)\b)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression languagePatchRegex(
        R"(\b(english|french|german|spanish|italian|portuguese|polish|russian|korean|chinese|arabic|hebrew)\b)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression patchQualifierRegex(
        R"(\b(v\d+(\.\d+)*|version\s*\d+(\.\d+)*|patch|addendum|redux|restoration|fix|improvement|update|final)\b)",
        QRegularExpression::CaseInsensitiveOption);

    if (explicitTranslationRegex.match(tag).hasMatch()) {
        return true;
    }

    return languagePatchRegex.match(tag).hasMatch() && patchQualifierRegex.match(tag).hasMatch();
}

bool looksHackTag(const QString &tag, bool bracketed)
{
    static const QRegularExpression hackRegex(
        R"(\b(hack|patched|patch|addendum|automap|redux|randomizer|restoration|rebalance|improvement|bugfix|uncensored|msu-?1|dx|deluxe|edition|hardtype|easytype|remix|overhaul)\b)",
        QRegularExpression::CaseInsensitiveOption);

    if (hackRegex.match(tag).hasMatch()) {
        return true;
    }

    return bracketed && !isOfficialRegionOrRevisionTag(tag) && !looksPrototypeTag(tag) && !looksHomebrewTag(tag);
}

QString joinPatchLabels(const QStringList &labels)
{
    QStringList deduped;
    for (const QString &label : labels) {
        if (!label.isEmpty() && !deduped.contains(label, Qt::CaseInsensitive)) {
            deduped.append(label);
        }
    }
    return deduped.join(QStringLiteral(" ")).simplified();
}

} // namespace

PatchedRomInfo PatchedRomParser::parse(const QString &nameOrPath)
{
    PatchedRomInfo info;
    info.rawName = stripTrailingExtension(nameOrPath);
    if (info.rawName.isEmpty()) {
        info.rawName = nameOrPath.trimmed();
    }

    info.baseTitle = info.rawName;

    const QRegularExpression tagRegex(R"((\([^)]*\)|\[[^\]]*\]))");
    QRegularExpressionMatchIterator it = tagRegex.globalMatch(info.rawName);

    QStringList patchLabels;
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString fullTag = match.captured(0);
        const QString tag = cleanTag(fullTag);
        const bool bracketed = fullTag.startsWith('[');

        if (tag.isEmpty()) {
            info.baseTitle.remove(fullTag);
            continue;
        }

        if (looksPrototypeTag(tag)) {
            info.fileType = QStringLiteral("prototype");
            info.baseTitle.remove(fullTag);
            continue;
        }

        if (looksHomebrewTag(tag)) {
            info.fileType = QStringLiteral("homebrew");
            info.baseTitle.remove(fullTag);
            continue;
        }

        if (looksTranslationTag(tag)) {
            info.fileType = QStringLiteral("translation");
            info.isPatched = true;
            patchLabels.append(tag);
            info.baseTitle.remove(fullTag);
            continue;
        }

        if (looksHackTag(tag, bracketed)) {
            if (info.fileType == QStringLiteral("official")) {
                info.fileType = QStringLiteral("hack");
            }
            info.isPatched = true;
            patchLabels.append(tag);
            info.baseTitle.remove(fullTag);
            continue;
        }

        if (isOfficialRegionOrRevisionTag(tag)) {
            info.baseTitle.remove(fullTag);
            continue;
        }

        info.baseTitle.remove(fullTag);
    }

    info.baseTitle = info.baseTitle.replace('_', ' ').replace('.', ' ').simplified();
    info.patchName = joinPatchLabels(patchLabels);
    if (!info.isPatched && !info.patchName.isEmpty()) {
        info.isPatched = true;
    }

    if (info.baseTitle.isEmpty()) {
        info.baseTitle = info.rawName.simplified();
    }

    return info;
}

} // namespace Remus