#pragma once

#include "../metadata/metadata_provider.h"

#include <QList>

namespace CompendiumEnrichmentMatchUtils {

inline const Remus::GameMetadata &bestMetadataCandidate(
    const QList<Remus::GameMetadata> &candidates, bool includeRating = false) {
    Q_ASSERT(!candidates.isEmpty());
    int bestScore = -1;
    int bestIdx = 0;
    for (int i = 0; i < candidates.size(); ++i) {
        const Remus::GameMetadata &c = candidates.at(i);
        int score = (!c.description.isEmpty() ? 1 : 0) + (!c.developer.isEmpty() ? 1 : 0)
            + (!c.publisher.isEmpty() ? 1 : 0) + (!c.genres.isEmpty() ? 1 : 0) + (c.releaseDate.size() >= 4 ? 1 : 0)
            + (c.players > 0 ? 1 : 0);
        if (includeRating && c.rating > 0.0f)
            ++score;
        if (score > bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }
    return candidates.at(bestIdx);
}

} // namespace CompendiumEnrichmentMatchUtils
