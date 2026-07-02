#include "metadata_title_normalize.h"

#include <QChar>

namespace Remus {
namespace MetadataTitleNormalize {

    namespace {

        QString stripTrailingParentheticalGroups(QString s) {
            while (s.endsWith(QLatin1Char(')'))) {
                const int paren = s.lastIndexOf(QLatin1Char('('));
                if (paren <= 0)
                    break;
                s = s.left(paren).trimmed();
            }
            return s;
        }

        QString stripLeadingArticles(QString s) {
            static const QStringList articles { QStringLiteral("the "), QStringLiteral("a "), QStringLiteral("an ") };
            for (const QString &art : articles) {
                if (s.startsWith(art)) {
                    s = s.mid(art.size());
                    break;
                }
            }
            return s;
        }

        QString compactAlnumLower(const QString &s) {
            QString out;
            out.reserve(s.size());
            for (const QChar &c : s) {
                if (c.isLetterOrNumber())
                    out.append(c);
            }
            return out;
        }

        QString normalizedTitleCore(QString s) {
            s = s.normalized(QString::NormalizationForm_KC).trimmed();
            s = stripTrailingParentheticalGroups(s);
            s = s.toLower();
            return stripLeadingArticles(s);
        }

    } // namespace

    QString metadataTitleMatchTokens(const QString &title) {
        QString s = normalizedTitleCore(title);
        QString out;
        out.reserve(s.size());
        bool pendingSpace = false;
        for (const QChar &c : s) {
            if (c.isLetterOrNumber()) {
                if (pendingSpace && !out.isEmpty()) {
                    out.append(QLatin1Char(' '));
                    pendingSpace = false;
                }
                out.append(c);
            } else {
                pendingSpace = !out.isEmpty();
            }
        }
        return out.simplified();
    }

    QStringList metadataTitleIndexKeys(const QString &title) {
        QStringList keys;
        const QString core = normalizedTitleCore(title);
        const QString compact = compactAlnumLower(core);
        if (!compact.isEmpty())
            keys.append(compact);

        const int colon = core.indexOf(QLatin1Char(':'));
        if (colon > 0) {
            const QString subtitleKey = compactAlnumLower(core.left(colon).trimmed());
            if (!subtitleKey.isEmpty() && !keys.contains(subtitleKey))
                keys.append(subtitleKey);
        }
        return keys;
    }

    QString normalizeMetadataTitle(const QString &title) {
        return compactAlnumLower(normalizedTitleCore(title));
    }

} // namespace MetadataTitleNormalize
} // namespace Remus
