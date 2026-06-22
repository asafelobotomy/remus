#pragma once

#include <QString>
#include <QStringList>

namespace CompendiumEnrichmentMame {

inline void appendRomNameCandidate(QStringList &candidates, const QString &alias) {
    if (alias.isEmpty() || candidates.contains(alias))
        return;
    if (!alias.contains(QLatin1Char(' ')) && alias.size() <= 32)
        candidates.prepend(alias);
    else
        candidates.append(alias);
}

} // namespace CompendiumEnrichmentMame
