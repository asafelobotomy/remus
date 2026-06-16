#include "compendium_provider.h"

#include "compendium_dat_extractor.h"

#include "../core/constants/confidence.h"
#include "../core/constants/match_methods.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include <algorithm>

namespace Remus {

namespace {

    using namespace Constants::Confidence::MultiSignal;

    QString normalizeSerial(const QString &serial) {
        QString s = serial.trimmed().toUpper();
        static const QRegularExpression prefixRe(
            QStringLiteral("^(?:MK-|HDR-|SDC-|T-|HKT-)"), QRegularExpression::CaseInsensitiveOption);
        s.replace(prefixRe, QString());
        static const QRegularExpression suffixRe(QStringLiteral("-\\d{1,2}$"));
        s.replace(suffixRe, QString());
        return s;
    }

    bool serialsMatch(const QString &a, const QString &b) {
        if (a.compare(b, Qt::CaseInsensitive) == 0) {
            return true;
        }
        const QString na = normalizeSerial(a);
        const QString nb = normalizeSerial(b);
        if (na.isEmpty() || nb.isEmpty()) {
            return false;
        }
        return na == nb;
    }

    QString romNameFromEntryKey(const QString &sourceEntryKey) {
        const QStringList parts = sourceEntryKey.split(QLatin1Char('|'));
        return parts.size() >= 3 ? parts.at(2).trimmed() : QString();
    }

    struct PayloadFields {
        QString romName;
        qint64 size = 0;
        QString serial;
    };

    PayloadFields parsePayloadJson(const QString &payloadJson) {
        PayloadFields fields;
        if (payloadJson.isEmpty()) {
            return fields;
        }
        const QJsonObject obj = QJsonDocument::fromJson(payloadJson.toUtf8()).object();
        fields.romName = obj.value(QStringLiteral("rom_name")).toString();
        const QJsonValue sizeVal = obj.value(QStringLiteral("size"));
        if (sizeVal.isDouble() || sizeVal.isString()) {
            fields.size = static_cast<qint64>(sizeVal.toDouble());
        }
        fields.serial = obj.value(QStringLiteral("serial")).toString();
        return fields;
    }

    PayloadFields loadPayloadFields(QSqlDatabase &db, const QString &sourceEntryKey) {
        PayloadFields fields;
        fields.romName = romNameFromEntryKey(sourceEntryKey);
        if (sourceEntryKey.isEmpty()) {
            return fields;
        }

        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT payload_json FROM source_items WHERE external_key = ? LIMIT 1"));
        query.addBindValue(sourceEntryKey);
        if (!query.exec() || !query.next()) {
            return fields;
        }

        const PayloadFields parsed = parsePayloadJson(query.value(0).toString());
        if (!parsed.romName.isEmpty()) {
            fields.romName = parsed.romName;
        }
        if (parsed.size > 0) {
            fields.size = parsed.size;
        }
        if (!parsed.serial.isEmpty()) {
            fields.serial = parsed.serial;
        }
        return fields;
    }

    void applyCorroboration(const ROMSignals &input, const PayloadFields &payload, const QString &entrySerial,
        CompendiumMultiSignalMatch &match) {
        const QString signalBase = QFileInfo(input.filename).completeBaseName().toLower();
        const QString entryBase
            = QFileInfo(payload.romName.isEmpty() ? match.romName : payload.romName).completeBaseName().toLower();
        if (!signalBase.isEmpty() && !entryBase.isEmpty() && signalBase == entryBase) {
            match.filenameMatch = true;
            match.confidenceScore += FILENAME_BONUS;
            match.matchSignalCount++;
        }

        const qint64 entrySize = payload.size > 0 ? payload.size : match.romSize;
        if (input.fileSize > 0 && entrySize > 0) {
            const qint64 sizeDiff = qAbs(input.fileSize - entrySize);
            if (sizeDiff <= SIZE_TOLERANCE) {
                match.sizeMatch = true;
                match.confidenceScore += SIZE_BONUS;
                match.matchSignalCount++;
            }
        }

        const QString candidateSerial = !entrySerial.isEmpty() ? entrySerial : payload.serial;
        if (!input.serial.isEmpty() && !candidateSerial.isEmpty() && serialsMatch(input.serial, candidateSerial)) {
            match.serialMatch = true;
            match.confidenceScore += SERIAL_BONUS;
            match.matchSignalCount++;
        }
    }

    bool lookupHashCandidates(QSqlDatabase &db, const QString &hashType, const QString &normalizedHash, int systemId,
        QList<CompendiumMultiSignalMatch> &matches, QString &matchedVia) {
        if (normalizedHash.isEmpty()) {
            return false;
        }

        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT gs.game_id, gs.source_entry_key "
                                     "FROM game_signatures gs "
                                     "JOIN games g ON g.game_id = gs.game_id "
                                     "WHERE gs.hash_type = ? AND gs.hash_value = ? "
                                     "AND (? = 0 OR g.system_id = ?)"));
        query.addBindValue(hashType);
        query.addBindValue(normalizedHash);
        query.addBindValue(systemId);
        query.addBindValue(systemId);
        if (!query.exec()) {
            qWarning() << "CompendiumProvider::matchROM hash query failed:" << query.lastError().text();
            return false;
        }

        bool found = false;
        while (query.next()) {
            CompendiumMultiSignalMatch match;
            match.gameId = query.value(0).toString();
            match.sourceEntryKey = query.value(1).toString();
            match.romName = romNameFromEntryKey(match.sourceEntryKey);
            match.hashMatch = true;
            match.confidenceScore = HASH_BASE;
            match.matchSignalCount = 1;
            match.matchedHash = hashType + QLatin1Char(':') + normalizedHash;
            matches.append(match);
            matchedVia = hashType.toUpper();
            found = true;
        }
        return found;
    }

} // namespace

QList<CompendiumMultiSignalMatch> CompendiumProvider::matchROM(const ROMSignals &input, const QString &system) const {
    QList<CompendiumMultiSignalMatch> matches;
    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return matches;
    }

    const int systemId = resolveSystemId(system);
    QString matchedVia;

    if (!input.crc32.isEmpty()) {
        const QString normalizedCrc = Compendium::DatExtractor::normalizeHash(input.crc32);
        lookupHashCandidates(db, QStringLiteral("crc32"), normalizedCrc, systemId, matches, matchedVia);
    }
    if (matches.isEmpty() && !input.md5.isEmpty()) {
        const QString normalizedMd5 = input.md5.trimmed().toLower();
        lookupHashCandidates(db, QStringLiteral("md5"), normalizedMd5, systemId, matches, matchedVia);
    }
    if (matches.isEmpty() && !input.sha1.isEmpty()) {
        const QString normalizedSha1 = input.sha1.trimmed().toLower();
        lookupHashCandidates(db, QStringLiteral("sha1"), normalizedSha1, systemId, matches, matchedVia);
    }

    if (!matches.isEmpty()) {
        for (CompendiumMultiSignalMatch &match : matches) {
            const PayloadFields payload = loadPayloadFields(db, match.sourceEntryKey);
            if (!payload.romName.isEmpty()) {
                match.romName = payload.romName;
            }
            if (payload.size > 0) {
                match.romSize = payload.size;
            }
            applyCorroboration(input, payload, payload.serial, match);
        }
    }

    if (matches.isEmpty() && input.fileSize > 0 && !input.filename.isEmpty()) {
        const QString signalBase = QFileInfo(input.filename).completeBaseName().toLower();
        QSqlQuery query(db);
        query.prepare(QStringLiteral("SELECT si.external_key, si.payload_json, gs.game_id "
                                     "FROM source_items si "
                                     "JOIN game_signatures gs ON gs.source_entry_key = si.external_key "
                                     "JOIN games g ON g.game_id = gs.game_id "
                                     "WHERE (? = 0 OR g.system_id = ?) "
                                     "AND json_extract(si.payload_json, '$.size') IS NOT NULL "
                                     "AND ABS(CAST(json_extract(si.payload_json, '$.size') AS INTEGER) - ?) <= ? "
                                     "LIMIT 100"));
        query.addBindValue(systemId);
        query.addBindValue(systemId);
        query.addBindValue(input.fileSize);
        query.addBindValue(SIZE_TOLERANCE);
        if (query.exec()) {
            QSet<QString> seenKeys;
            while (query.next()) {
                const QString entryKey = query.value(0).toString();
                if (seenKeys.contains(entryKey)) {
                    continue;
                }
                seenKeys.insert(entryKey);

                const PayloadFields payload = parsePayloadJson(query.value(1).toString());
                const QString romName = payload.romName.isEmpty() ? romNameFromEntryKey(entryKey) : payload.romName;
                const QString entryBase = QFileInfo(romName).completeBaseName().toLower();
                if (entryBase != signalBase) {
                    continue;
                }

                CompendiumMultiSignalMatch match;
                match.gameId = query.value(2).toString();
                match.sourceEntryKey = entryKey;
                match.romName = romName;
                match.romSize = payload.size;
                match.filenameMatch = true;
                match.sizeMatch = true;
                match.confidenceScore = FILENAME_SIZE_BASE;
                match.matchSignalCount = 2;
                const QString candidateSerial = payload.serial;
                if (!input.serial.isEmpty() && !candidateSerial.isEmpty()
                    && serialsMatch(input.serial, candidateSerial)) {
                    match.serialMatch = true;
                    match.confidenceScore += SERIAL_BONUS;
                    match.matchSignalCount++;
                }
                matches.append(match);
                matchedVia = QStringLiteral("filename+size");
                break;
            }
        }
    }

    if (matches.isEmpty() && !input.serial.isEmpty()) {
        const QString normalizedSerial = input.serial.toUpper().trimmed();
        QSet<QString> seenEntries;

        auto appendSerialMatch = [&](const QString &gameId, const QString &entryKey, const QString &entrySerial) {
            const QString dedupeKey = gameId + QLatin1Char('|') + entryKey;
            if (seenEntries.contains(dedupeKey)) {
                return;
            }
            seenEntries.insert(dedupeKey);

            CompendiumMultiSignalMatch match;
            match.gameId = gameId;
            match.sourceEntryKey = entryKey;
            match.serial = entrySerial;
            match.romName = romNameFromEntryKey(entryKey);
            match.serialMatch = true;
            match.confidenceScore = SERIAL_BASE;
            match.matchSignalCount = 1;

            const PayloadFields payload = loadPayloadFields(db, entryKey);
            if (!payload.romName.isEmpty()) {
                match.romName = payload.romName;
            }
            if (payload.size > 0) {
                match.romSize = payload.size;
            }
            applyCorroboration(input, payload, entrySerial, match);
            matches.append(match);
            matchedVia = QStringLiteral("serial");
        };

        QSqlQuery exactSerial(db);
        exactSerial.prepare(QStringLiteral("SELECT gs.game_id, gs.source_entry_key, gs.serial_value "
                                           "FROM game_serials gs "
                                           "JOIN games g ON g.game_id = gs.game_id "
                                           "WHERE gs.serial_value = ? "
                                           "AND (? = 0 OR g.system_id = ?)"));
        exactSerial.addBindValue(normalizedSerial);
        exactSerial.addBindValue(systemId);
        exactSerial.addBindValue(systemId);
        if (exactSerial.exec()) {
            while (exactSerial.next()) {
                appendSerialMatch(
                    exactSerial.value(0).toString(), exactSerial.value(1).toString(), exactSerial.value(2).toString());
            }
        }

        if (matches.isEmpty()) {
            QSqlQuery fuzzySerial(db);
            fuzzySerial.prepare(QStringLiteral("SELECT gs.game_id, gs.source_entry_key, gs.serial_value "
                                               "FROM game_serials gs "
                                               "JOIN games g ON g.game_id = gs.game_id "
                                               "WHERE (? = 0 OR g.system_id = ?)"));
            fuzzySerial.addBindValue(systemId);
            fuzzySerial.addBindValue(systemId);
            if (fuzzySerial.exec()) {
                while (fuzzySerial.next()) {
                    const QString entrySerial = fuzzySerial.value(2).toString();
                    if (!serialsMatch(input.serial, entrySerial)) {
                        continue;
                    }
                    appendSerialMatch(fuzzySerial.value(0).toString(), fuzzySerial.value(1).toString(), entrySerial);
                }
            }
        }
    }

    std::sort(
        matches.begin(), matches.end(), [](const CompendiumMultiSignalMatch &a, const CompendiumMultiSignalMatch &b) {
            return a.confidenceScore > b.confidenceScore;
        });

    if (!matches.isEmpty()) {
        qDebug() << "Compendium matchROM:" << input.filename << "→" << matches.first().gameId << "via" << matchedVia
                 << "(" << matches.first().confidencePercent() << "%)";
    }

    return matches;
}

GameMetadata CompendiumProvider::metadataFromMatch(
    const CompendiumMultiSignalMatch &match, const QString &system) const {
    Q_UNUSED(system);

    GameMetadata metadata = fetchGameMetadata(match.gameId);
    if (metadata.id.isEmpty()) {
        return { };
    }

    metadata.providerId = QStringLiteral("compendium");

    const QStringList entryParts = match.sourceEntryKey.split(QLatin1Char('|'));
    if (entryParts.size() >= 2) {
        const QString romTitle = entryParts.at(1).trimmed();
        if (!romTitle.isEmpty()) {
            metadata.title = romTitle;
        }
    }
    if (!match.romName.isEmpty()) {
        metadata.externalIds.insert(QStringLiteral("rom_name"), match.romName);
    }
    if (!match.serial.isEmpty()) {
        metadata.externalIds.insert(QStringLiteral("serial"), match.serial);
    }

    if (match.hashMatch) {
        metadata.matchScore = 1.0f;
        metadata.matchMethod = QString::fromLatin1(Constants::MatchMethods::HASH);
    } else if (match.serialMatch && !match.filenameMatch) {
        metadata.matchScore = static_cast<float>(match.confidencePercent()) / 100.0f;
        metadata.matchMethod = QStringLiteral("serial");
    } else {
        metadata.matchScore = static_cast<float>(match.confidencePercent()) / 100.0f;
        metadata.matchMethod = match.confidencePercent() >= Constants::Confidence::Thresholds::EXACT_NAME
            ? QString::fromLatin1(Constants::MatchMethods::NAME)
            : QString::fromLatin1(Constants::MatchMethods::FUZZY);
    }

    return metadata;
}

} // namespace Remus
