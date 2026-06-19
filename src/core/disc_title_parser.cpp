#include "disc_title_parser.h"

#include "constants/files.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace Remus {

namespace {

    const QStringList &knownLabelExtensions() {
        static const QStringList kExtensions = []() {
            QStringList exts = Constants::Files::CHD_SOURCE_EXTENSIONS + Constants::Files::EXTRACTABLE_DISC_EXTENSIONS
                + Constants::Files::ARCHIVE_EXTENSIONS + Constants::Files::PRIMARY_DISC_EXTENSIONS
                + Constants::Files::M3U_SOURCE_EXTENSIONS + Constants::Files::PBP_SOURCE_EXTENSIONS
                + Constants::Files::RVZ_SOURCE_EXTENSIONS + Constants::Files::CSO_SOURCE_EXTENSIONS
                + Constants::Files::WBFS_SOURCE_EXTENSIONS
                + QStringList { Constants::Files::PBP, Constants::Files::GCM, Constants::Files::GCZ,
                      Constants::Files::WAD, Constants::Files::CCD, Constants::Files::MDS, Constants::Files::CDI,
                      Constants::Files::RAW, Constants::Files::SUB, Constants::Files::ECM, Constants::Files::ISZ,
                      Constants::Files::DOL, Constants::Files::DAT, Constants::Files::LST, Constants::Files::ELF,
                      QStringLiteral(".sfc"), QStringLiteral(".smc"), QStringLiteral(".nes"), QStringLiteral(".md"),
                      QStringLiteral(".gen"), QStringLiteral(".gb"), QStringLiteral(".gbc"), QStringLiteral(".gba"),
                      QStringLiteral(".nds"), QStringLiteral(".n64"), QStringLiteral(".z64"), QStringLiteral(".v64") };
            for (QString &ext : exts)
                ext = ext.trimmed().toLower();
            return exts;
        }();
        return kExtensions;
    }

    QString stripExtensionIfPresent(const QString &title) {
        const QString trimmed = title.trimmed();
        const QFileInfo info(trimmed);
        const QString suffix = info.suffix();
        if (suffix.isEmpty())
            return trimmed;

        const QString dottedSuffix = QStringLiteral(".") + suffix.toLower();
        if (!Constants::Files::containsExtension(knownLabelExtensions(), dottedSuffix))
            return trimmed;
        return info.completeBaseName().trimmed();
    }

    bool isLikelyRegionTag(const QString &tag) {
        static const QStringList kKnownRegions = {
            QStringLiteral("usa"),
            QStringLiteral("us"),
            QStringLiteral("europe"),
            QStringLiteral("eu"),
            QStringLiteral("japan"),
            QStringLiteral("jp"),
            QStringLiteral("world"),
            QStringLiteral("canada"),
            QStringLiteral("australia"),
            QStringLiteral("brazil"),
            QStringLiteral("korea"),
            QStringLiteral("germany"),
            QStringLiteral("france"),
            QStringLiteral("spain"),
            QStringLiteral("italy"),
            QStringLiteral("netherlands"),
            QStringLiteral("sweden"),
            QStringLiteral("norway"),
            QStringLiteral("denmark"),
            QStringLiteral("finland"),
            QStringLiteral("china"),
            QStringLiteral("asia"),
            QStringLiteral("pal"),
            QStringLiteral("ntsc"),
            QStringLiteral("secam"),
        };

        const QString lower = tag.trimmed().toLower();
        if (kKnownRegions.contains(lower))
            return true;

        static const QRegularExpression languageList(
            QStringLiteral("^[a-z]{2}(?:,[a-z]{2})+$"), QRegularExpression::CaseInsensitiveOption);
        if (languageList.match(lower).hasMatch())
            return true;

        static const QRegularExpression regionCombo(
            QStringLiteral("^(usa|eu|japan|world|asia)(?:,(usa|eu|japan|world|asia))*$"),
            QRegularExpression::CaseInsensitiveOption);
        return regionCombo.match(lower).hasMatch();
    }

    QString detectSetRole(const QString &titleLower) {
        if (titleLower.contains(QStringLiteral("cd-audio")) || titleLower.contains(QStringLiteral("the music"))
            || titleLower.contains(QStringLiteral("soundtrack")) || titleLower.contains(QStringLiteral("audio disc"))) {
            return QStringLiteral("audio");
        }
        if (titleLower.contains(QStringLiteral("making disc")) || titleLower.contains(QStringLiteral("bonus disc"))
            || titleLower.contains(QStringLiteral("demo disc")) || titleLower.contains(QStringLiteral("prototype"))) {
            return QStringLiteral("bonus");
        }
        if (titleLower.contains(QStringLiteral("install disc")) || titleLower.contains(QStringLiteral("data disc"))) {
            return QStringLiteral("data");
        }
        return QStringLiteral("game");
    }

    QString extractBracketVariant(const QString &title) {
        static const QRegularExpression trailingTags(
            QStringLiteral("(\\[[^\\]]+\\])\\s*$"), QRegularExpression::CaseInsensitiveOption);
        QString variant;
        QString working = title;
        QRegularExpressionMatch match = trailingTags.match(working);
        while (match.hasMatch()) {
            const QString tag = match.captured(1);
            const QString inner = tag.mid(1, tag.size() - 2).trimmed();
            if (!inner.isEmpty() && inner != QLatin1String("!") && inner != QLatin1String("b")) {
                if (!variant.isEmpty())
                    variant += QLatin1Char('|');
                variant += inner;
            }
            working.chop(match.capturedLength(0));
            working = working.trimmed();
            match = trailingTags.match(working);
        }
        return variant;
    }

    void stripDiscMarker(QString &workingTitle, int &discNumber, int &discCount) {
        static const QRegularExpression parenDisc(
            QStringLiteral("\\((?:Disc|CD|Disk)\\s*(\\d+)(?:\\s+of\\s+(\\d+))?\\s*\\)"),
            QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression bareDisc(
            QStringLiteral("\\b(?:Disc|CD|Disk)\\s*(\\d+)(?:\\s+of\\s+(\\d+))?\\b"),
            QRegularExpression::CaseInsensitiveOption);

        QRegularExpressionMatch match;
        int matchStart = -1;
        int matchLength = 0;

        QRegularExpressionMatchIterator it = parenDisc.globalMatch(workingTitle);
        while (it.hasNext()) {
            match = it.next();
            matchStart = match.capturedStart(0);
            matchLength = match.capturedLength(0);
        }

        if (matchStart < 0) {
            QRegularExpressionMatchIterator bareIt = bareDisc.globalMatch(workingTitle);
            while (bareIt.hasNext()) {
                match = bareIt.next();
                matchStart = match.capturedStart(0);
                matchLength = match.capturedLength(0);
            }
        }

        if (matchStart < 0 || !match.hasMatch())
            return;

        discNumber = match.captured(1).toInt();
        if (match.lastCapturedIndex() >= 2 && !match.captured(2).isEmpty())
            discCount = match.captured(2).toInt();

        workingTitle.remove(matchStart, matchLength);
        workingTitle = workingTitle.trimmed();
    }

    QString stripTrailingParentheticalSubtitle(QString &workingTitle) {
        static const QRegularExpression trailingParen(QStringLiteral("\\s*\\(([^)]+)\\)\\s*$"));
        const QRegularExpressionMatch match = trailingParen.match(workingTitle);
        if (!match.hasMatch())
            return QString();

        const QString inner = match.captured(1).trimmed();
        if (inner.isEmpty() || isLikelyRegionTag(inner))
            return QString();

        workingTitle.remove(match.capturedStart(0), match.capturedLength(0));
        return inner;
    }

    QString cleanupBaseTitle(QString baseTitle) {
        baseTitle = baseTitle.trimmed();
        baseTitle.replace(QRegularExpression(QStringLiteral("\\s{2,}")), QStringLiteral(" "));
        baseTitle.replace(QRegularExpression(QStringLiteral("\\(\\s*\\)")), QString());
        return baseTitle.trimmed();
    }

} // namespace

bool DiscTitleParser::isMultiDisc(const QString &label) {
    static const QRegularExpression re(
        QStringLiteral("\\b(Disc|CD|Disk)\\s*\\d+"), QRegularExpression::CaseInsensitiveOption);
    return re.match(label).hasMatch();
}

QString DiscTitleParser::extractBaseTitle(const QString &label) {
    return parseTitle(label).baseTitle;
}

int DiscTitleParser::extractDiscNumber(const QString &label) {
    return parseTitle(label).discNumber;
}

int DiscTitleParser::extractDiscCount(const QString &label) {
    return parseTitle(label).discCount;
}

DiscTitleInfo DiscTitleParser::parseTitle(const QString &title) {
    DiscTitleInfo info;
    info.rawTitle = title.trimmed();
    info.setRole = detectSetRole(info.rawTitle.toLower());

    QString working = stripExtensionIfPresent(info.rawTitle);
    info.setVariant = extractBracketVariant(working);
    if (!info.setVariant.isEmpty()) {
        static const QRegularExpression trailingTags(QStringLiteral("\\s*(\\[[^\\]]+\\])\\s*"));
        working.remove(trailingTags);
        working = working.trimmed();
    }

    stripDiscMarker(working, info.discNumber, info.discCount);
    info.isMultiDisc = info.discNumber > 0;

    info.pathSubtitle = stripTrailingParentheticalSubtitle(working);

    // Strip common trailing region tags for display base title.
    static const QRegularExpression trailingRegion(QStringLiteral("\\s*\\(([^)]+)\\)\\s*$"));
    QRegularExpressionMatch regionMatch = trailingRegion.match(working);
    while (regionMatch.hasMatch()) {
        const QString inner = regionMatch.captured(1).trimmed();
        if (!isLikelyRegionTag(inner))
            break;
        working.remove(regionMatch.capturedStart(0), regionMatch.capturedLength(0));
        working = working.trimmed();
        regionMatch = trailingRegion.match(working);
    }

    info.baseTitle = cleanupBaseTitle(working);

    // Set-key seed: base title + split-path subtitle only. Bracket variants and disc
    // indices must not affect identityBase — those live in set_variant / disc_number.
    QString setKeySeed = info.baseTitle;
    if (!info.pathSubtitle.isEmpty()) {
        if (!setKeySeed.isEmpty())
            setKeySeed += QLatin1Char(' ');
        setKeySeed += info.pathSubtitle;
    }
    info.identityBase = normalizeForIdentity(setKeySeed);
    return info;
}

QString DiscTitleParser::normalizeForIdentity(const QString &raw) {
    QString s = raw.toLower().trimmed();
    s = stripExtensionIfPresent(s);

    static const QRegularExpression rePunct(QStringLiteral("[^a-z0-9 ]"));
    s.remove(rePunct);

    static const QRegularExpression reSpaces(QStringLiteral(" {2,}"));
    s.replace(reSpaces, QStringLiteral(" "));

    if (s.startsWith(QStringLiteral("the ")))
        s = s.mid(4);
    if (s.startsWith(QStringLiteral("a ")))
        s = s.mid(2);

    static const QRegularExpression reDisc(QStringLiteral("\\b(?:disc|disk)\\s*[a-z0-9]+(?:\\s+of\\s+[a-z0-9]+)?\\s*$"
                                                          "|\\bcd\\s*\\d+(?:\\s+of\\s+\\d+)?\\s*$"));
    s.remove(reDisc);

    static const QRegularExpression trailingRegion(
        QStringLiteral("\\s*\\((usa|eu|europe|japan|world|pal|ntsc)\\)\\s*$"));
    s.remove(trailingRegion);

    return s.trimmed();
}

} // namespace Remus
