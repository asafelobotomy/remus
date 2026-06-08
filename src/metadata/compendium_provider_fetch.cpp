#include "compendium_provider.h"

#include "../core/constants/match_methods.h"
#include "../core/constants/providers.h"
#include "thumbnail_url_helper.h"

#include <QDate>
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace Remus {

namespace {

    QString resolvedValue(const QMap<QString, QString> &facts, std::initializer_list<const char *> keys,
        const QString &fallback = QString()) {
        for (const char *key : keys) {
            const QString fieldName = QString::fromLatin1(key);
            const QString value = facts.value(fieldName).trimmed();
            if (!value.isEmpty()) {
                return value;
            }
        }

        return fallback.trimmed();
    }

    int resolvedIntValue(
        const QMap<QString, QString> &facts, std::initializer_list<const char *> keys, int fallback = 0) {
        bool ok = false;
        const QString value = resolvedValue(facts, keys);
        if (!value.isEmpty()) {
            const int parsed = value.toInt(&ok);
            if (ok) {
                return parsed;
            }
        }

        return fallback;
    }

    float resolvedFloatValue(
        const QMap<QString, QString> &facts, std::initializer_list<const char *> keys, float fallback = 0.0f) {
        bool ok = false;
        const QString value = resolvedValue(facts, keys);
        if (!value.isEmpty()) {
            const float parsed = value.toFloat(&ok);
            if (ok) {
                return parsed;
            }
        }

        return fallback;
    }

    int releaseYearFromDate(const QString &releaseDate) {
        if (releaseDate.size() == 4) {
            bool ok = false;
            const int year = releaseDate.toInt(&ok);
            return ok ? year : 0;
        }

        const QDate parsed = QDate::fromString(releaseDate, Qt::ISODate);
        return parsed.isValid() ? parsed.year() : 0;
    }

} // namespace

GameMetadata CompendiumProvider::fetchGameMetadata(const QString &gameId) const {
    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return { };
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT g.game_id, g.canonical_title, g.primary_region_code, g.release_date, g.release_year, "
        "       g.developer, g.publisher, g.genre, g.players_max, g.description, g.rating, s.internal_name "
        "FROM games g "
        "JOIN systems s ON s.system_id = g.system_id "
        "WHERE g.game_id = ? LIMIT 1"));
    query.addBindValue(gameId);
    if (!query.exec() || !query.next()) {
        return { };
    }

    const QMap<QString, QString> facts = loadResolvedFacts(gameId);

    GameMetadata metadata;
    metadata.id = query.value(0).toString();
    metadata.title = resolvedValue(facts, { "canonical_title", "title" }, query.value(1).toString());
    metadata.region = resolvedValue(facts, { "primary_region_code", "region" }, query.value(2).toString());
    metadata.releaseDate = resolvedValue(facts, { "release_date" }, query.value(3).toString());
    if (metadata.releaseDate.isEmpty()) {
        const int releaseYear = resolvedIntValue(facts, { "release_year" }, query.value(4).toInt());
        if (releaseYear > 0) {
            metadata.releaseDate = QString::number(releaseYear);
        }
    }
    metadata.developer = resolvedValue(facts, { "developer" }, query.value(5).toString());
    metadata.publisher = resolvedValue(facts, { "publisher" }, query.value(6).toString());
    metadata.description = resolvedValue(facts, { "description" }, query.value(9).toString());
    metadata.players = resolvedIntValue(facts, { "players_max" }, query.value(8).toInt());
    metadata.rating = resolvedFloatValue(facts, { "rating" }, query.value(10).toFloat());
    metadata.system = query.value(11).toString();
    metadata.providerId = QString::fromLatin1(Constants::Providers::COMPENDIUM);
    metadata.fetchedAt = QDateTime::currentDateTimeUtc();

    const QString genre = resolvedValue(facts, { "genre" }, query.value(7).toString());
    if (!genre.isEmpty()) {
        metadata.genres = QStringList { genre };
    }
    if (metadata.rating > 0.0f) {
        metadata.ratingSource = QStringLiteral("Compendium");
    }

    populateExternalIds(metadata, metadata.id);
    return metadata;
}

QMap<QString, QString> CompendiumProvider::loadResolvedFacts(const QString &gameId) const {
    QMap<QString, QString> facts;

    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return facts;
    }

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT cr.field_name, gf.field_value "
                                 "FROM canonical_resolution cr "
                                 "JOIN game_facts gf ON gf.fact_id = cr.selected_fact_id "
                                 "WHERE cr.game_id = ?"));
    query.addBindValue(gameId);
    if (!query.exec()) {
        return facts;
    }

    while (query.next()) {
        facts.insert(query.value(0).toString(), query.value(1).toString());
    }

    return facts;
}

void CompendiumProvider::populateExternalIds(GameMetadata &metadata, const QString &gameId) const {
    QSqlDatabase db = database();
    if (!db.isOpen()) {
        return;
    }

    QSqlQuery signatures(db);
    signatures.prepare(QStringLiteral("SELECT hash_type, hash_value FROM game_signatures WHERE game_id = ?"));
    signatures.addBindValue(gameId);
    if (signatures.exec()) {
        while (signatures.next()) {
            const QString hashType = signatures.value(0).toString();
            if (!metadata.externalIds.contains(hashType)) {
                metadata.externalIds.insert(hashType, signatures.value(1).toString());
            }
        }
    }

    QSqlQuery serials(db);
    serials.prepare(QStringLiteral("SELECT serial_value FROM game_serials WHERE game_id = ? ORDER BY serial_id"));
    serials.addBindValue(gameId);
    if (serials.exec()) {
        while (serials.next()) {
            const QString sv = serials.value(0).toString();
            if (!sv.isEmpty())
                metadata.serials.append(sv);
        }
    }
}

} // namespace Remus
