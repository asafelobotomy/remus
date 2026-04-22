#include "provider_orchestrator.h"

#include "metadata_cache.h"

#include <QDebug>

#include "../core/constants/constants.h"
#include "../core/constants/provider_fields.h"
#include "../core/logging_categories.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logMetadata)
#define qInfo() qCInfo(logMetadata)
#define qWarning() qCWarning(logMetadata)
#define qCritical() qCCritical(logMetadata)

namespace Remus {

using namespace Constants;

namespace {

void mergeMetadata(GameMetadata &target, const GameMetadata &source)
{
    if (target.title.isEmpty() && !source.title.isEmpty()) target.title = source.title;
    if (target.system.isEmpty() && !source.system.isEmpty()) target.system = source.system;
    if (target.region.isEmpty() && !source.region.isEmpty()) target.region = source.region;
    if (target.publisher.isEmpty() && !source.publisher.isEmpty()) target.publisher = source.publisher;
    if (target.developer.isEmpty() && !source.developer.isEmpty()) target.developer = source.developer;
    if (target.genres.isEmpty() && !source.genres.isEmpty()) target.genres = source.genres;
    if (target.releaseDate.isEmpty() && !source.releaseDate.isEmpty()) target.releaseDate = source.releaseDate;
    if (target.description.isEmpty() && !source.description.isEmpty()) target.description = source.description;
    if (target.players == 0 && source.players != 0) target.players = source.players;
    if (target.rating == 0.0f && source.rating != 0.0f) target.rating = source.rating;
    if (target.ratingSource.isEmpty() && !source.ratingSource.isEmpty()) target.ratingSource = source.ratingSource;
    if (target.id.isEmpty() && !source.id.isEmpty()) target.id = source.id;
    if (target.boxArtUrl.isEmpty() && !source.boxArtUrl.isEmpty()) target.boxArtUrl = source.boxArtUrl;
    if (target.screenshotUrls.isEmpty() && !source.screenshotUrls.isEmpty()) target.screenshotUrls = source.screenshotUrls;
    for (auto it = source.externalIds.constBegin(); it != source.externalIds.constEnd(); ++it) {
        if (!target.externalIds.contains(it.key())) {
            target.externalIds[it.key()] = it.value();
        }
    }
    if (target.providerId.isEmpty() && !source.providerId.isEmpty()) target.providerId = source.providerId;
    if (!target.fetchedAt.isValid() && source.fetchedAt.isValid()) target.fetchedAt = source.fetchedAt;
    if (target.matchScore == 0.0f && source.matchScore > 0.0f) target.matchScore = source.matchScore;
    if (target.matchMethod.isEmpty() && !source.matchMethod.isEmpty()) target.matchMethod = source.matchMethod;
}

} // namespace

// ---------------------------------------------------------------------------
// Field-targeted enrichment cascade
// ---------------------------------------------------------------------------

// static
ProviderOrchestrator::FieldSet ProviderOrchestrator::computeFieldGap(const GameMetadata &m)
{
    using namespace Constants::ProviderFields;
    ProviderOrchestrator::FieldSet gap;
    if (m.title.isEmpty())       gap.insert(TITLE);
    if (m.publisher.isEmpty())   gap.insert(PUBLISHER);
    if (m.developer.isEmpty())   gap.insert(DEVELOPER);
    if (m.releaseDate.isEmpty()) gap.insert(RELEASE_DATE);
    if (m.genres.isEmpty())      gap.insert(GENRES);
    if (m.players == 0)          gap.insert(PLAYERS);
    if (m.description.isEmpty()) gap.insert(DESCRIPTION);
    if (m.boxArtUrl.isEmpty())   gap.insert(BOX_ART_URL);
    return gap;
}

GameMetadata ProviderOrchestrator::enrichMissingFields(const FieldSet &missing,
                                                        const GameMetadata &existing,
                                                        const QString &hash,
                                                        const QString &name,
                                                        const QString &system,
                                                        const QString &crc32,
                                                        const QString &md5,
                                                        const QString &sha1,
                                                        const QString &serial)
{
    if (missing.isEmpty()) {
        qInfo() << "enrichMissingFields: no gaps — skipping all providers";
        return existing;
    }

    // Check cache first — a cached record may already fill all gaps.
    if (!hash.isEmpty() && m_cache) {
        const GameMetadata cached = m_cache->getByHash(hash, system);
        if (!cached.title.isEmpty()) {
            GameMetadata merged = existing;
            mergeMetadata(merged, cached);
            const FieldSet remaining = computeFieldGap(merged);
            if (remaining.isEmpty()) {
                qInfo() << "enrichMissingFields: cache hit fills all gaps for" << hash;
                return merged;
            }
        }
    }

    GameMetadata accumulator = existing;
    FieldSet gapSet = missing;

    // LOCAL providers first (priority 200–150).
    const QStringList localProviders = getSortedLocalProviders();
    for (const QString &providerName : localProviders) {
        const QSet<QString> &caps =
            Constants::ProviderFields::CAPABILITIES.value(providerName.toLower());
        if (!caps.intersects(gapSet)) {
            qInfo() << "enrichMissingFields: skipping local provider" << providerName
                    << "(no capability overlap with gap)";
            continue;
        }
        queryProvider(accumulator, providerName, hash, name, system, crc32, md5, sha1, serial);
        gapSet = computeFieldGap(accumulator);
        if (gapSet.isEmpty()) {
            qInfo() << "enrichMissingFields: all gaps filled after local provider" << providerName;
            break;
        }
    }

    // REMOTE providers — only if gaps remain after local pass.
    if (!gapSet.isEmpty()) {
        const QStringList remoteProviders = getSortedRemoteProviders();
        for (const QString &providerName : remoteProviders) {
            const QSet<QString> &caps =
                Constants::ProviderFields::CAPABILITIES.value(providerName.toLower());
            if (!caps.intersects(gapSet)) {
                qInfo() << "enrichMissingFields: skipping remote provider" << providerName
                        << "(no capability overlap with gap)";
                continue;
            }
            queryProvider(accumulator, providerName, hash, name, system, crc32, md5, sha1, serial);
            gapSet = computeFieldGap(accumulator);
            if (gapSet.isEmpty()) {
                qInfo() << "enrichMissingFields: all gaps filled after remote provider" << providerName;
                break;
            }
        }
    }

    if (accumulator.title.isEmpty()) {
        qWarning() << "enrichMissingFields: all providers exhausted, no title for:" << name;
        emit allProvidersFailed();
        return existing;
    }

    if (!gapSet.isEmpty()) {
        qInfo() << "enrichMissingFields: remaining gaps after all providers:"
                << QStringList(gapSet.constBegin(), gapSet.constEnd()).join(", ");
    }

    if (!hash.isEmpty() && m_cache) {
        m_cache->store(accumulator, hash, system);
    }
    return accumulator;
}

} // namespace Remus
