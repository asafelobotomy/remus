#include "compendium_enrichment.h"
#include "compendium_enrichment_sql.h"
#include "compendium_progress.h"

#include "launchbox_platform_map.h"
#include "title_similarity.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVector>
#include <QXmlStreamReader>

#include <algorithm>

using CompendiumEnrichmentSql::EnrichmentBatchWriter;
using CompendiumEnrichmentSql::FactInsertSpec;
using CompendiumEnrichmentSql::FactReplaceQueries;
using CompendiumEnrichmentSql::SnapshotSpec;
using CompendiumEnrichmentSql::SourceSpec;
using namespace CompendiumEnrichmentSql;
using namespace Remus::TitleSimilarity;
using namespace Remus::LaunchBoxPlatformMap;

namespace CompendiumEnrichment {

namespace {

    constexpr int kFuzzyCandidateCap = 64;

    struct LaunchBoxEntry {
        QString platform;
        QString filename;
        QString title;
        QString normalizedTitle;
        QString matchTokens;
        QString titlePrefix;
        QString developer;
        QString publisher;
        QString releaseDate;
        QString overview;
        QString genre;
        int maxPlayers = 0;
        QString esrb;
        int databaseId = 0;
        QList<int> systemIds;
    };

    struct LaunchBoxPlatformBucket {
        QHash<QString, QSet<int>> byExactTitle;
        QHash<QString, QSet<int>> byTitlePrefix;
        QSet<int> allEntries;
    };

    struct LaunchBoxIndex {
        QVector<LaunchBoxEntry> entries;
        QHash<int, LaunchBoxPlatformBucket> bySystemId;
        QHash<QString, QSet<int>> byFilenameKey;
        QSet<QString> unmappedPlatforms;
    };

    struct LaunchBoxMatchResult {
        const LaunchBoxEntry *entry = nullptr;
        QString method;
        float score = 0.0f;
        MatchTier tier = MatchTier::Reject;
    };

    struct MatchStats {
        int filenameExact = 0;
        int titleExact = 0;
        int titleExactSubtitle = 0;
        int fuzzyAccept = 0;
        int review = 0;
        int reject = 0;
        int noMatch = 0;
    };

    QString normalizeFilenameKey(const QString &path) {
        const QString base = QFileInfo(path.trimmed()).fileName().toLower();
        QString key = base;
        key.remove(QRegularExpression(QStringLiteral("[^a-z0-9.]")));
        return key;
    }

    QString firstTitlePrefix(const QString &matchTokens) {
        for (const QString &token : tokenizeForMatching(matchTokens)) {
            if (token.size() >= 3)
                return token;
        }
        return QString();
    }

    void insertIntoBucket(LaunchBoxPlatformBucket &bucket, const LaunchBoxEntry &entry, int idx) {
        bucket.allEntries.insert(idx);

        for (const QString &key : metadataTitleIndexKeys(entry.title))
            bucket.byExactTitle[key].insert(idx);

        if (!entry.titlePrefix.isEmpty())
            bucket.byTitlePrefix[entry.titlePrefix].insert(idx);
    }

    void insertLaunchBoxEntry(LaunchBoxIndex &index, LaunchBoxEntry entry) {
        entry.normalizedTitle = normalizeMetadataTitle(entry.title);
        entry.matchTokens = metadataTitleMatchTokens(entry.title);
        entry.titlePrefix = firstTitlePrefix(entry.matchTokens);

        entry.systemIds = resolveSystemIds(entry.platform);
        if (entry.systemIds.isEmpty())
            index.unmappedPlatforms.insert(entry.platform);

        const int idx = index.entries.size();
        index.entries.append(std::move(entry));
        const LaunchBoxEntry &stored = index.entries.constLast();

        if (!stored.filename.isEmpty()) {
            const QString fileKey = normalizeFilenameKey(stored.filename);
            if (!fileKey.isEmpty())
                index.byFilenameKey[fileKey].insert(idx);
        }

        if (stored.systemIds.isEmpty())
            return;

        for (int systemId : stored.systemIds) {
            LaunchBoxPlatformBucket &bucket = index.bySystemId[systemId];
            insertIntoBucket(bucket, stored, idx);
        }
    }

    bool buildLaunchBoxIndex(const QString &xmlPath, LaunchBoxIndex &index, QString &error) {
        reportCompendiumEnrichmentProgress(
            QStringLiteral("indexing"), 0, 0, QStringLiteral("parsing %1").arg(QFileInfo(xmlPath).fileName()));

        QFile file(xmlPath);
        if (!file.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("Could not open LaunchBox metadata XML: %1").arg(xmlPath);
            return false;
        }

        QXmlStreamReader xml(&file);
        LaunchBoxEntry current;
        QString currentElement;
        int gameCount = 0;

        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                currentElement = xml.name().toString();
                if (currentElement == QStringLiteral("Game"))
                    current = LaunchBoxEntry { };
            } else if (xml.isCharacters() && !xml.isWhitespace()) {
                const QString text = xml.text().toString().trimmed();
                if (text.isEmpty())
                    continue;
                if (currentElement == QStringLiteral("ApplicationPath") || currentElement == QStringLiteral("FilePath"))
                    current.filename = text;
                else if (currentElement == QStringLiteral("Platform"))
                    current.platform = text;
                else if (currentElement == QStringLiteral("Title") || currentElement == QStringLiteral("Name"))
                    current.title = text;
                else if (currentElement == QStringLiteral("Developer"))
                    current.developer = text;
                else if (currentElement == QStringLiteral("Publisher"))
                    current.publisher = text;
                else if (currentElement == QStringLiteral("ReleaseDate"))
                    current.releaseDate = text;
                else if (currentElement == QStringLiteral("Overview")
                    || currentElement == QStringLiteral("Description"))
                    current.overview = text;
                else if (currentElement == QStringLiteral("Genre") || currentElement == QStringLiteral("Genres"))
                    current.genre = text;
                else if (currentElement == QStringLiteral("MaxPlayers")) {
                    bool ok = false;
                    const int players = text.toInt(&ok);
                    if (ok && players > 0)
                        current.maxPlayers = players;
                } else if (currentElement == QStringLiteral("ESRB"))
                    current.esrb = text;
                else if (currentElement == QStringLiteral("DatabaseID")) {
                    bool ok = false;
                    const int id = text.toInt(&ok);
                    if (ok && id > 0)
                        current.databaseId = id;
                }
            } else if (xml.isEndElement() && xml.name() == QStringLiteral("Game")) {
                if (!current.platform.isEmpty() && (!current.filename.isEmpty() || !current.title.isEmpty())) {
                    insertLaunchBoxEntry(index, current);
                    ++gameCount;
                    if (gameCount % 10000 == 0) {
                        qInfo().noquote() << QStringLiteral("[LaunchBox] Indexed %1 games …").arg(gameCount);
                        reportCompendiumEnrichmentProgress(
                            QStringLiteral("indexing"), gameCount, 0, QStringLiteral("games parsed"));
                    }
                }
            }
        }

        if (xml.hasError()) {
            error = QStringLiteral("LaunchBox XML parse error at line %1: %2")
                        .arg(xml.lineNumber())
                        .arg(xml.errorString());
            return false;
        }

        qInfo().noquote() << QStringLiteral(
            "[LaunchBox] Indexed %1 games from %2 (%3 platform buckets, %4 unmapped platforms)")
                                 .arg(gameCount)
                                 .arg(xmlPath)
                                 .arg(index.bySystemId.size())
                                 .arg(index.unmappedPlatforms.size());
        reportCompendiumEnrichmentProgress(QStringLiteral("indexing"), gameCount, gameCount,
            QStringLiteral("%1 platform buckets").arg(index.bySystemId.size()));
        if (!index.unmappedPlatforms.isEmpty()) {
            QStringList sample = index.unmappedPlatforms.values();
            std::sort(sample.begin(), sample.end());
            const int sampleCount = std::min(static_cast<int>(sample.size()), 10);
            qInfo().noquote() << QStringLiteral("[LaunchBox] Unmapped platform sample: %1")
                                     .arg(sample.mid(0, sampleCount).join(QStringLiteral(", ")));
        }
        return gameCount > 0;
    }

    QString normalizeReleaseDate(const QString &raw) {
        const QString trimmed = raw.trimmed();
        if (trimmed.size() >= 10 && trimmed.at(4) == QLatin1Char('-'))
            return trimmed.left(10);
        bool ok = false;
        const int year = trimmed.toInt(&ok);
        if (ok && year > 1970 && year < 2030)
            return QStringLiteral("%1-01-01").arg(year);
        return QString();
    }

    const LaunchBoxEntry *entryAt(const LaunchBoxIndex &index, int idx) {
        if (idx < 0 || idx >= index.entries.size())
            return nullptr;
        return &index.entries.at(idx);
    }

    QList<int> collectCandidates(const LaunchBoxPlatformBucket &bucket, const QString &matchTokens) {
        QSet<int> candidates;
        const QString prefix = firstTitlePrefix(matchTokens);
        if (!prefix.isEmpty()) {
            const auto it = bucket.byTitlePrefix.constFind(prefix);
            if (it != bucket.byTitlePrefix.constEnd()) {
                for (int idx : *it)
                    candidates.insert(idx);
            }
        }

        if (candidates.size() < kFuzzyCandidateCap) {
            for (int idx : bucket.allEntries) {
                candidates.insert(idx);
                if (candidates.size() >= kFuzzyCandidateCap)
                    break;
            }
        }
        return QList<int>(candidates.begin(), candidates.end());
    }

    const LaunchBoxEntry *lookupByFilename(
        const LaunchBoxIndex &index, int systemId, const QString &systemName, const QString &romName) {
        const QString filenameKey = normalizeFilenameKey(romName);
        if (filenameKey.isEmpty())
            return nullptr;

        const auto bucketIt = index.byFilenameKey.constFind(filenameKey);
        if (bucketIt == index.byFilenameKey.constEnd())
            return nullptr;

        for (int idx : *bucketIt) {
            const LaunchBoxEntry *entry = entryAt(index, idx);
            if (!entry)
                continue;
            if (entry->systemIds.contains(systemId))
                return entry;
            if (platformKeysCompatible(systemName, entry->platform))
                return entry;
        }
        return nullptr;
    }

    LaunchBoxMatchResult matchLaunchBoxEntry(const LaunchBoxIndex &index, int systemId, const QString &systemName,
        const QString &canonicalTitle, const QString &romName, MatchStats &stats) {
        LaunchBoxMatchResult result;

        if (!romName.isEmpty()) {
            const LaunchBoxEntry *filenameMatch = lookupByFilename(index, systemId, systemName, romName);
            if (filenameMatch) {
                result.entry = filenameMatch;
                result.method = QStringLiteral("filename_exact");
                result.score = 1.0f;
                result.tier = MatchTier::Exact;
                ++stats.filenameExact;
                return result;
            }
        }

        const auto bucketIt = index.bySystemId.constFind(systemId);
        if (bucketIt == index.bySystemId.constEnd()) {
            ++stats.noMatch;
            return result;
        }
        const LaunchBoxPlatformBucket &bucket = *bucketIt;

        const QStringList titleKeys = metadataTitleIndexKeys(canonicalTitle);
        const QString matchTokens = metadataTitleMatchTokens(canonicalTitle);

        for (int variant = 0; variant < titleKeys.size(); ++variant) {
            const auto exactIt = bucket.byExactTitle.constFind(titleKeys.at(variant));
            if (exactIt == bucket.byExactTitle.constEnd() || exactIt->isEmpty())
                continue;
            result.entry = entryAt(index, *exactIt->constBegin());
            result.method = variant == 0 ? QStringLiteral("title_exact") : QStringLiteral("title_exact_subtitle");
            result.score = 1.0f;
            result.tier = MatchTier::Exact;
            if (variant == 0)
                ++stats.titleExact;
            else
                ++stats.titleExactSubtitle;
            return result;
        }

        struct RankedCandidate {
            int idx = -1;
            Scores scores;
            MatchTier tier = MatchTier::Reject;
        };
        QVector<RankedCandidate> ranked;
        ranked.reserve(kFuzzyCandidateCap);

        for (int idx : collectCandidates(bucket, matchTokens)) {
            const LaunchBoxEntry *entry = entryAt(index, idx);
            if (!entry || entry->matchTokens.isEmpty())
                continue;

            RankedCandidate candidate;
            candidate.idx = idx;
            candidate.scores = scorePair(matchTokens, entry->matchTokens);
            candidate.tier = classifyConservative(candidate.scores, false);
            if (candidate.tier == MatchTier::Reject)
                continue;
            ranked.append(candidate);
        }

        if (ranked.isEmpty()) {
            ++stats.noMatch;
            return result;
        }

        std::sort(ranked.begin(), ranked.end(), [](const RankedCandidate &a, const RankedCandidate &b) {
            if (a.tier != b.tier)
                return static_cast<int>(a.tier) < static_cast<int>(b.tier);
            if (qFuzzyCompare(a.scores.combined, b.scores.combined))
                return a.scores.jaroWinkler > b.scores.jaroWinkler;
            return a.scores.combined > b.scores.combined;
        });

        const RankedCandidate &best = ranked.constFirst();
        if (best.tier == MatchTier::Review) {
            ++stats.reject;
            return result;
        }

        if (ranked.size() >= 2) {
            const RankedCandidate &second = ranked.at(1);
            if (second.tier == MatchTier::HighConfidence
                && (best.scores.tokenSet - second.scores.tokenSet) < Thresholds::AmbiguityTokenSetDelta) {
                ++stats.reject;
                return result;
            }
        }

        result.entry = entryAt(index, best.idx);
        result.score = best.scores.combined;
        result.tier = best.tier;
        if (best.scores.tokenSet >= best.scores.jaroWinkler)
            result.method = QStringLiteral("fuzzy_token_set");
        else
            result.method = QStringLiteral("fuzzy_jaro_winkler");
        ++stats.fuzzyAccept;
        return result;
    }

} // anonymous namespace

bool enrichFromLaunchBox(QSqlDatabase &database, const QString &metadataXmlPath, int &gamesEnriched, int &factsInserted,
    QString &error, const QStringList &gapFieldFilter) {
    gamesEnriched = 0;
    factsInserted = 0;

    if (metadataXmlPath.isEmpty() || !QFile::exists(metadataXmlPath)) {
        qInfo() << "[LaunchBox] Metadata.xml not found — enrichment skipped";
        return true;
    }

    LaunchBoxIndex index;
    if (!buildLaunchBoxIndex(metadataXmlPath, index, error))
        return false;

    const QString gapSql = gameMetadataGapSqlForFields(gapFieldFilter);
    const QString pendingSql = launchBoxPendingGamesSql(gapSql);

    QSqlQuery countQ(database);
    if (!countQ.exec(QStringLiteral("SELECT COUNT(*) FROM (%1)").arg(pendingSql))) {
        error = QStringLiteral("Count LaunchBox candidates: %1").arg(countQ.lastError().text());
        return false;
    }
    int pendingTotal = 0;
    if (countQ.next())
        pendingTotal = countQ.value(0).toInt();
    countQ.finish();

    if (pendingTotal == 0) {
        qInfo() << "[LaunchBox] No games with metadata gaps require enrichment";
        return true;
    }

    qInfo().noquote() << QStringLiteral("[LaunchBox] Matching %1 pending games …").arg(pendingTotal);
    reportCompendiumEnrichmentProgress(
        QStringLiteral("matching"), 0, pendingTotal, QStringLiteral("starting match loop"));

    const QString sourceId = QStringLiteral("launchbox");
    const QString snapshotId = QStringLiteral("launchbox-bulk");

    const QSet<QString> skipGameIds = loadGamesWithMinSourceFieldFacts(database, sourceId, 4, error);
    if (!error.isEmpty())
        return false;
    const QSet<QString> noMatchGameIds = loadGamesWithLaunchBoxNoMatchFacts(database, error);
    if (!error.isEmpty())
        return false;
    if (!skipGameIds.isEmpty()) {
        qInfo().noquote()
            << QStringLiteral("[LaunchBox] Skipping %1 games already enriched by source").arg(skipGameIds.size());
    }
    if (!noMatchGameIds.isEmpty()) {
        qInfo().noquote()
            << QStringLiteral("[LaunchBox] Skipping %1 games with prior no_match").arg(noMatchGameIds.size());
    }

    bool bulkCleared = false;
    bool enrichmentSourceReady = false;
    auto ensureEnrichmentReady = [&]() -> bool {
        if (!enrichmentSourceReady) {
            if (!upsertEnrichmentSource(database,
                    SourceSpec {
                        sourceId,
                        QStringLiteral("LaunchBox Games Database"),
                        QStringLiteral("static-file"),
                        QStringLiteral("https://gamesdb.launchbox-app.com/"),
                        /*attributionRequired=*/true,
                        /*priority=*/45,
                        QString(),
                    },
                    SnapshotSpec {
                        snapshotId,
                        QFileInfo(metadataXmlPath).fileName(),
                    },
                    error)) {
                return false;
            }
            enrichmentSourceReady = true;
        }
        if (!bulkCleared) {
            if (!bulkClearSourceFactBlockers(database, sourceId, error))
                return false;
            bulkCleared = true;
        }
        return true;
    };

    QSqlQuery updateQ(database);
    updateQ.prepare(QStringLiteral("UPDATE games SET "
                                   "description  = COALESCE(NULLIF(description, ''), ?), "
                                   "genre        = COALESCE(NULLIF(genre, ''), ?), "
                                   "developer    = COALESCE(NULLIF(developer, ''), ?), "
                                   "publisher    = COALESCE(NULLIF(publisher, ''), ?), "
                                   "release_year = COALESCE(release_year, ?), "
                                   "release_date = COALESCE(release_date, ?), "
                                   "players_max  = COALESCE(players_max, ?), "
                                   "age_rating   = COALESCE(NULLIF(age_rating, ''), ?) "
                                   "WHERE game_id = ?"));

    QSqlQuery factQ(database);
    factQ.prepare(QStringLiteral("INSERT INTO game_facts "
                                 "(game_id, field_name, field_value, value_type, source_id, snapshot_id, "
                                 "source_priority, confidence) "
                                 "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));

    QSqlQuery delQ(database);
    delQ.prepare(QStringLiteral("DELETE FROM game_facts WHERE game_id = ? AND field_name = ? AND source_id = ?"));

    const FactInsertSpec factSpec {
        sourceId,
        snapshotId,
        45,
        0.70,
    };
    FactReplaceQueries replaceQueries(database);
    EnrichmentBatchWriter batchWriter(database);

    auto insertFact = [&](const QString &gameId, const QString &field, const QString &value,
                          const QString &type = QStringLiteral("text"), float confidence = 0.70f) -> bool {
        bool inserted = false;
        FactInsertSpec spec = factSpec;
        spec.confidence = confidence;
        if (!insertGameFact(replaceQueries, delQ, factQ, spec, gameId, field, value, type, error,
                QStringLiteral("launchbox"), &inserted)) {
            return false;
        }
        if (inserted)
            ++factsInserted;
        return true;
    };

    MatchStats stats;
    int matched = 0;
    int processed = 0;
    int skipped = 0;

    QSqlQuery pendingQ(database);
    if (!pendingQ.exec(pendingSql)) {
        error = QStringLiteral("Query LaunchBox candidates: %1").arg(pendingQ.lastError().text());
        return false;
    }

    while (pendingQ.next()) {
        const QString gameId = pendingQ.value(0).toString();
        const int systemId = pendingQ.value(1).toInt();
        const QString systemName = pendingQ.value(2).toString();
        const QString canonicalTitle = pendingQ.value(3).toString().trimmed();
        const QString romName = pendingQ.value(4).toString().trimmed();

        ++processed;
        if (processed % 2500 == 0 || processed == pendingTotal) {
            qInfo().noquote() << QStringLiteral(
                "[LaunchBox] Processed %1 / %2 pending games (%3 matched, %4 skipped) …")
                                     .arg(processed)
                                     .arg(pendingTotal)
                                     .arg(matched)
                                     .arg(skipped);
            reportCompendiumEnrichmentProgress(QStringLiteral("matching"), processed, pendingTotal,
                QStringLiteral("%1 matched, %2 skipped").arg(matched).arg(skipped), gamesEnriched, factsInserted);
        }

        if (skipGameIds.contains(gameId) || noMatchGameIds.contains(gameId)) {
            ++skipped;
            if (!batchWriter.onGameProcessed(error))
                return false;
            continue;
        }

        const LaunchBoxMatchResult match
            = matchLaunchBoxEntry(index, systemId, systemName, canonicalTitle, romName, stats);
        if (!match.entry) {
            const bool hasPlatformBucket = index.bySystemId.contains(systemId);
            if (hasPlatformBucket) {
                if (!ensureEnrichmentReady())
                    return false;
                QJsonObject noMatchProv;
                noMatchProv.insert(QStringLiteral("source"), QStringLiteral("launchbox"));
                noMatchProv.insert(QStringLiteral("tier"), QStringLiteral("no_match"));
                noMatchProv.insert(
                    QStringLiteral("attempted_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
                const QString noMatchJson
                    = QString::fromUtf8(QJsonDocument(noMatchProv).toJson(QJsonDocument::Compact));
                if (!insertFact(
                        gameId, QStringLiteral("enrichment_match"), noMatchJson, QStringLiteral("json"), 0.10f)) {
                    return false;
                }
            }
            if (!batchWriter.onGameProcessed(error))
                return false;
            continue;
        }

        const LaunchBoxEntry *entry = match.entry;
        const float factConfidence
            = match.tier == MatchTier::Exact ? 0.95f : std::min(0.92f, std::max(0.75f, match.score));

        if (!ensureEnrichmentReady())
            return false;

        const QString releaseDate = normalizeReleaseDate(entry->releaseDate);
        int releaseYear = 0;
        if (releaseDate.size() >= 4) {
            bool ok = false;
            const int y = releaseDate.left(4).toInt(&ok);
            if (ok && y > 1970 && y < 2030)
                releaseYear = y;
        }

        updateQ.bindValue(0, nullableText(entry->overview));
        updateQ.bindValue(1, nullableText(entry->genre));
        updateQ.bindValue(2, nullableText(entry->developer));
        updateQ.bindValue(3, nullableText(entry->publisher));
        updateQ.bindValue(4, nullableInt(releaseYear));
        updateQ.bindValue(5, nullableText(releaseDate));
        updateQ.bindValue(6, nullableInt(entry->maxPlayers));
        updateQ.bindValue(7, nullableText(entry->esrb));
        updateQ.bindValue(8, gameId);
        if (!execPrepared(updateQ, error, QStringLiteral("Update game launchbox")))
            return false;
        if (updateQ.numRowsAffected() > 0)
            ++gamesEnriched;

        const QString yearStr = releaseYear > 0 ? QString::number(releaseYear) : QString();
        const QString playersStr = entry->maxPlayers > 0 ? QString::number(entry->maxPlayers) : QString();
        const QString databaseIdStr = entry->databaseId > 0 ? QString::number(entry->databaseId) : QString();

        QJsonObject provenance;
        provenance.insert(QStringLiteral("method"), match.method);
        provenance.insert(QStringLiteral("score"), static_cast<double>(match.score));
        provenance.insert(QStringLiteral("tier"),
            match.tier == MatchTier::Exact ? QStringLiteral("exact") : QStringLiteral("high_confidence"));
        provenance.insert(QStringLiteral("launchbox_title"), entry->title);
        const QString provenanceJson = QString::fromUtf8(QJsonDocument(provenance).toJson(QJsonDocument::Compact));

        if (!insertFact(gameId, QStringLiteral("description"), entry->overview, QStringLiteral("text"), factConfidence)
            || !insertFact(gameId, QStringLiteral("genre"), entry->genre, QStringLiteral("text"), factConfidence)
            || !insertFact(
                gameId, QStringLiteral("developer"), entry->developer, QStringLiteral("text"), factConfidence)
            || !insertFact(
                gameId, QStringLiteral("publisher"), entry->publisher, QStringLiteral("text"), factConfidence)
            || !insertFact(gameId, QStringLiteral("release_year"), yearStr, QStringLiteral("integer"), factConfidence)
            || !insertFact(gameId, QStringLiteral("release_date"), releaseDate, QStringLiteral("text"), factConfidence)
            || !insertFact(gameId, QStringLiteral("players_max"), playersStr, QStringLiteral("integer"), factConfidence)
            || !insertFact(gameId, QStringLiteral("age_rating"), entry->esrb, QStringLiteral("text"), factConfidence)
            || !insertFact(
                gameId, QStringLiteral("launchbox_id"), databaseIdStr, QStringLiteral("integer"), factConfidence)
            || !insertFact(
                gameId, QStringLiteral("enrichment_match"), provenanceJson, QStringLiteral("json"), factConfidence)) {
            return false;
        }

        ++matched;
        if (!batchWriter.onGameProcessed(error))
            return false;
    }

    if (!batchWriter.finish(error))
        return false;

    qInfo().noquote() << QStringLiteral(
        "[LaunchBox] Match stats: filename_exact=%1 title_exact=%2 title_exact_subtitle=%3 "
        "fuzzy_accept=%4 reject=%5 no_match=%6")
                             .arg(stats.filenameExact)
                             .arg(stats.titleExact)
                             .arg(stats.titleExactSubtitle)
                             .arg(stats.fuzzyAccept)
                             .arg(stats.reject)
                             .arg(stats.noMatch);
    qInfo().noquote() << QStringLiteral("[LaunchBox] Matched %1 / %2 pending games (%3 skipped)")
                             .arg(matched)
                             .arg(pendingTotal)
                             .arg(skipped);
    return true;
}

} // namespace CompendiumEnrichment
