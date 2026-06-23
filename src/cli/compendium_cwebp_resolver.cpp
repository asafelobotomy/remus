#include "compendium_cwebp_resolver.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace CompendiumCwebp {

QString resolveCwebpExecutable(const QString &repoRoot) {
    const QString systemPath = QStandardPaths::findExecutable(QStringLiteral("cwebp"));
    if (!systemPath.isEmpty()) {
        return systemPath;
    }

    if (repoRoot.isEmpty()) {
        return { };
    }

    const QStringList candidates {
        QDir(repoRoot).filePath(QStringLiteral("node_modules/.bin/cwebp")),
        QDir(repoRoot).filePath(QStringLiteral("node_modules/cwebp-bin/vendor/cwebp")),
    };

    for (const QString &candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isExecutable()) {
            return info.absoluteFilePath();
        }
    }

    return { };
}

bool isAvailable(const QString &repoRoot) {
    return !resolveCwebpExecutable(repoRoot).isEmpty();
}

} // namespace CompendiumCwebp
