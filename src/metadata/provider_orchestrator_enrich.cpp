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

// ---------------------------------------------------------------------------
// Field-targeted enrichment cascade
// ---------------------------------------------------------------------------

// static
ProviderOrchestrator::FieldSet ProviderOrchestrator::computeFieldGap(const GameMetadata &m) {
    using namespace Constants::ProviderFields;
    ProviderOrchestrator::FieldSet gap;
    if (m.title.isEmpty())
        gap.insert(TITLE);
    if (m.publisher.isEmpty())
        gap.insert(PUBLISHER);
    if (m.developer.isEmpty())
        gap.insert(DEVELOPER);
    if (m.releaseDate.isEmpty())
        gap.insert(RELEASE_DATE);
    if (m.genres.isEmpty())
        gap.insert(GENRES);
    if (m.players == 0)
        gap.insert(PLAYERS);
    if (m.description.isEmpty())
        gap.insert(DESCRIPTION);
    if (m.boxArtUrl.isEmpty())
        gap.insert(BOX_ART_URL);
    if (m.screenshotUrls.isEmpty())
        gap.insert(SCREENSHOTS);
    if (m.rating == 0.0f)
        gap.insert(RATING);
    if (m.externalIds.isEmpty())
        gap.insert(EXTERNAL_IDS);
    return gap;
}

GameMetadata ProviderOrchestrator::enrichMissingFields(const FieldSet &missing, const GameMetadata &existing,
    const QString &hash, const QString &name, const QString &system, const QString &crc32, const QString &md5,
    const QString &sha1, const QString &serial, const QSet<QString> &excludeProviders, const QString &raMd5) {
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
        if (excludeProviders.contains(providerName)) {
            continue;
        }
        const QSet<QString> &caps = Constants::ProviderFields::CAPABILITIES.value(providerName.toLower());
        if (!caps.intersects(gapSet)) {
            qInfo() << "enrichMissingFields: skipping local provider" << providerName
                    << "(no capability overlap with gap)";
            continue;
        }
        queryProvider(accumulator, providerName, hash, name, system, crc32, md5, sha1, serial, 0, raMd5);
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
            if (excludeProviders.contains(providerName)) {
                continue;
            }
            const QSet<QString> &caps = Constants::ProviderFields::CAPABILITIES.value(providerName.toLower());
            if (!caps.intersects(gapSet)) {
                qInfo() << "enrichMissingFields: skipping remote provider" << providerName
                        << "(no capability overlap with gap)";
                continue;
            }
            queryProvider(accumulator, providerName, hash, name, system, crc32, md5, sha1, serial, 0, raMd5);
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
