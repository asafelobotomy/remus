#pragma once

#include "disc_set_completeness.h"

#include <QList>
#include <QString>

class QSqlDatabase;

namespace Remus {

struct CompendiumFileDiscContext {
    bool found = false;
    QString setKey;
    int discNumber = 0;
    int discCount = 0;
    QString compendiumGameId;
    QString setVariant;
    QString titleDisc;
};

/** Aggregated catalog topology for one canonical @c set_key. */
struct CatalogDiscSetSummary {
    bool found = false;
    QString titleDisc;
    QString baseTitle;
    int catalogDiscCount = 0;
};

QString resolveBundledCompendiumDbPath();

bool lookupCompendiumDiscContext(const QString &compendiumDbPath, const QString &systemInternalName,
    const QString &crc32, const QString &md5, const QString &sha1, CompendiumFileDiscContext &out);

bool lookupCompendiumDiscContextFromDb(QSqlDatabase &compendiumDb, const QString &systemInternalName,
    const QString &crc32, const QString &md5, const QString &sha1, CompendiumFileDiscContext &out);

bool compendiumDiscSetsAvailable(QSqlDatabase &compendiumDb);

bool lookupCatalogDiscSetSummary(QSqlDatabase &compendiumDb, const QString &setKey, CatalogDiscSetSummary &out);

QList<DiscSetCompletenessReport> computeDiscSetCompleteness(QSqlDatabase &compendiumDb, QSqlDatabase &libraryDb,
    const QString &compendiumGameId, const QList<int> &ownedFileIds);

DiscSetCompletenessReport computeDiscSetCompletenessBySetKey(QSqlDatabase &compendiumDb, QSqlDatabase &libraryDb,
    const QString &setKey, const QList<int> &ownedFileIds);

} // namespace Remus
