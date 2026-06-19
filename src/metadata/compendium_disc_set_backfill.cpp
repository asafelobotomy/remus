#include "compendium_disc_set_backfill.h"

#include "../core/disc_title_parser.h"
#include "compendium_disc_set_inserter.h"
#include "compendium_normalizer.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QVector>

namespace Remus {
namespace Compendium {

    namespace {

        int trackIndexFromRomName(const QString &romName) {
            static const QRegularExpression trackRe(QStringLiteral(R"((?:\(|\[|\b)(?:Track|Disc)\s*(\d+)(?:\)|\]|\b))"),
                QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch match = trackRe.match(romName);
            if (match.hasMatch())
                return match.captured(1).toInt();
            return 1;
        }

        QString primaryContentSha1FromPayload(const QJsonObject &payload, const QString &romName) {
            const QString lowerRom = romName.toLower();
            if (!lowerRom.endsWith(QStringLiteral(".chd")) && !lowerRom.endsWith(QStringLiteral(".rvz")))
                return { };
            const QString sha1 = payload.value(QStringLiteral("sha1")).toString().trimmed();
            return sha1.size() == 40 ? sha1.toLower() : QString();
        }

        bool envelopeFromSourceItemRow(const QSqlQuery &row, SourceRecordEnvelope &rec, QString &error) {
            Q_UNUSED(error);

            rec = SourceRecordEnvelope { };
            rec.sourceId = row.value(QStringLiteral("source_id")).toString();
            rec.snapshotId = row.value(QStringLiteral("snapshot_id")).toString();
            rec.externalKey = row.value(QStringLiteral("external_key")).toString();
            rec.systemHint = row.value(QStringLiteral("system_hint")).toString();
            rec.regionRaw = row.value(QStringLiteral("region_raw")).toString();
            rec.linkedGameId = row.value(QStringLiteral("game_id")).toString();
            rec.resolvedSystemId = row.value(QStringLiteral("system_id")).toInt();
            rec.resolvedRegionCode = row.value(QStringLiteral("primary_region_code")).toString();
            rec.payloadJson = row.value(QStringLiteral("payload_json")).toString();

            const QStringList parts = rec.externalKey.split(QLatin1Char('|'));
            if (parts.size() < 3)
                return false;

            if (rec.systemHint.isEmpty())
                rec.systemHint = parts.at(0);
            rec.titleRaw = parts.at(1);
            rec.datGameBlockName = parts.at(1);
            rec.datRomName = parts.at(2);
            rec.trackIndex = trackIndexFromRomName(rec.datRomName);

            QJsonObject payload;
            if (!rec.payloadJson.isEmpty()) {
                const QJsonDocument doc = QJsonDocument::fromJson(rec.payloadJson.toUtf8());
                if (doc.isObject())
                    payload = doc.object();
            }
            if (payload.contains(QStringLiteral("game_name")))
                rec.datGameBlockName = payload.value(QStringLiteral("game_name")).toString();
            if (payload.contains(QStringLiteral("rom_name")))
                rec.datRomName = payload.value(QStringLiteral("rom_name")).toString();
            if (rec.titleRaw.isEmpty() && payload.contains(QStringLiteral("game_name")))
                rec.titleRaw = payload.value(QStringLiteral("game_name")).toString();

            const DiscTitleInfo titleInfo = DiscTitleParser::parseTitle(rec.datGameBlockName);
            rec.parsedDiscNumber = titleInfo.discNumber;
            rec.parsedDiscCount = titleInfo.discCount;
            rec.parsedSetVariant = titleInfo.setVariant.isNull() ? QStringLiteral("") : titleInfo.setVariant;
            rec.parsedSetRole = titleInfo.setRole.isEmpty() ? QStringLiteral("game") : titleInfo.setRole;
            rec.primaryContentSha1 = primaryContentSha1FromPayload(payload, rec.datRomName);

            return !rec.linkedGameId.isEmpty() && rec.resolvedSystemId > 0 && !rec.datGameBlockName.isEmpty();
        }

        void inferDiscCounts(QList<SourceRecordEnvelope> &records) {
            QMap<QString, int> maxDiscNumber;
            QMap<QString, int> explicitDiscCount;

            for (const SourceRecordEnvelope &rec : records) {
                if (rec.resolvedSystemId <= 0)
                    continue;
                const DiscTitleInfo info = DiscTitleParser::parseTitle(rec.datGameBlockName);
                const QString identityKey = info.identityBase;
                maxDiscNumber[identityKey] = qMax(maxDiscNumber.value(identityKey, 0), rec.parsedDiscNumber);
                if (rec.parsedDiscCount > 0)
                    explicitDiscCount[identityKey] = qMax(explicitDiscCount.value(identityKey, 0), rec.parsedDiscCount);
            }

            for (SourceRecordEnvelope &rec : records) {
                if (rec.parsedDiscCount > 0)
                    continue;
                const DiscTitleInfo info = DiscTitleParser::parseTitle(rec.datGameBlockName);
                const int explicitCount = explicitDiscCount.value(info.identityBase, 0);
                const int maxDisc = maxDiscNumber.value(info.identityBase, 0);
                int count = explicitCount > 0 ? explicitCount : maxDisc;
                if (count <= 0)
                    count = 1;
                rec.parsedDiscCount = count;
            }
        }

    } // namespace

    bool DiscSetBackfill::backfillDiscSets(QSqlDatabase &db, bool clearExisting, CompilerStats &stats, QString &error) {
        if (clearExisting) {
            QSqlQuery clearTracks(db);
            if (!clearTracks.exec(QStringLiteral("DELETE FROM game_disc_tracks"))) {
                error = clearTracks.lastError().text();
                return false;
            }
            QSqlQuery clearSets(db);
            if (!clearSets.exec(QStringLiteral("DELETE FROM game_disc_sets"))) {
                error = clearSets.lastError().text();
                return false;
            }
        }

        QSqlQuery q(db);
        if (!q.exec(QStringLiteral(R"(
            SELECT si.source_id,
                   si.snapshot_id,
                   si.external_key,
                   si.system_hint,
                   si.region_raw,
                   si.payload_json,
                   gs.game_id,
                   g.system_id,
                   g.primary_region_code
            FROM source_items si
            INNER JOIN game_signatures gs
                ON gs.source_id = si.source_id
               AND gs.source_entry_key = si.external_key
            INNER JOIN games g ON g.game_id = gs.game_id
            WHERE g.system_id > 0
            GROUP BY si.source_item_id
            ORDER BY si.source_id, si.external_key
        )"))) {
            error = q.lastError().text();
            return false;
        }

        QList<SourceRecordEnvelope> records;
        while (q.next()) {
            SourceRecordEnvelope rec;
            if (!envelopeFromSourceItemRow(q, rec, error))
                continue;
            records.append(rec);
        }

        if (records.isEmpty()) {
            error = QStringLiteral("no source items with linked signatures found for backfill");
            return false;
        }

        inferDiscCounts(records);

        const CompendiumNormalizer normalizer;
        for (SourceRecordEnvelope &rec : records)
            normalizer.normalize(rec);

        return DiscSetInserter::insertDiscTopologyForRecords(records, db, stats, error);
    }

} // namespace Compendium
} // namespace Remus
