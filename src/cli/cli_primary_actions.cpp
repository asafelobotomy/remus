#include "cli_primary_actions.h"

#include <QHash>

namespace Remus {

namespace {

    QString canonicalPrimaryAction(const QString &name) {
        static const QHash<QString, QString> aliases = {
            { QStringLiteral("s"), QStringLiteral("scan") },
            { QStringLiteral("l"), QStringLiteral("list") },
            { QStringLiteral("m"), QStringLiteral("metadata") },
            { QStringLiteral("h"), QStringLiteral("help") },
        };

        return aliases.value(name, name);
    }

    bool isMetaAction(const QString &canonical) {
        return canonical == QStringLiteral("help") || canonical == QStringLiteral("version");
    }

} // namespace

QStringList collectPrimaryActions(const QCommandLineParser &parser, const QSet<QString> &actionOptions) {
    QStringList actions;
    QSet<QString> seen;

    for (const QString &name : actionOptions) {
        if (!parser.isSet(name))
            continue;

        const QString canonical = canonicalPrimaryAction(name);
        if (isMetaAction(canonical) || seen.contains(canonical))
            continue;

        seen.insert(canonical);
        actions.append(canonical);
    }

    return actions;
}

int validatePrimaryActionCombination(const QStringList &actions, QString *errorOut) {
    QStringList primary;
    for (const QString &action : actions) {
        const QString canonical = canonicalPrimaryAction(action);
        if (!isMetaAction(canonical))
            primary.append(canonical);
    }

    if (primary.size() <= 1)
        return 0;

    static const QSet<QString> kPipelineRoots = {
        QStringLiteral("process"),
        QStringLiteral("library"),
    };
    for (const QString &root : kPipelineRoots) {
        if (primary.contains(root)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Primary action '%1' cannot be combined with other actions: %2")
                                .arg(root, primary.join(QStringLiteral(", ")));
            }
            return kCliUsageError;
        }
    }

    static const QSet<QString> kScanCompanions = {
        QStringLiteral("scan"),
        QStringLiteral("hash"),
        QStringLiteral("match"),
        QStringLiteral("enrich"),
        QStringLiteral("bundle"),
        QStringLiteral("organize"),
        QStringLiteral("generate-m3u"),
        QStringLiteral("download-artwork"),
    };

    if (primary.size() > 1) {
        bool allModActions = true;
        for (const QString &action : primary) {
            if (!action.startsWith(QStringLiteral("mod-"))) {
                allModActions = false;
                break;
            }
        }
        if (allModActions)
            return 0;
    }

    if (primary.contains(QStringLiteral("scan"))) {
        for (const QString &action : primary) {
            if (!kScanCompanions.contains(action)) {
                if (errorOut) {
                    *errorOut = QStringLiteral("Action '%1' cannot be combined with --scan in one invocation. "
                                               "Allowed scan companions: %2")
                                    .arg(action, QStringList(kScanCompanions.values()).join(QStringLiteral(", ")));
                }
                return kCliUsageError;
            }
        }
        return 0;
    }

    if (errorOut) {
        *errorOut = QStringLiteral(
            "Only one primary action is allowed per invocation (except scan pipeline companions). Conflicting: %1")
                        .arg(primary.join(QStringLiteral(", ")));
    }
    return kCliUsageError;
}

} // namespace Remus
