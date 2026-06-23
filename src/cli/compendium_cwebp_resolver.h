#pragma once

#include <QString>

namespace CompendiumCwebp {

/** System PATH first, then repo node_modules/.bin/cwebp from npm cwebp-bin. */
QString resolveCwebpExecutable(const QString &repoRoot = QString());

bool isAvailable(const QString &repoRoot = QString());

} // namespace CompendiumCwebp
