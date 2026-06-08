#include "cli_commands.h"
#include "cli_helpers.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>

#include "../core/constants/systems.h"

using namespace Remus::Constants::Systems;

// ── --update-dats ─────────────────────────────────────────────────────────────
int handleUpdateDatsCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("update-dats"))
        return 0;

    qInfo() << "";
    qInfo() << "ℹ  Raw DAT management is no longer required.";
    qInfo() << "   Verification catalogs (No-Intro / Redump) are bundled in the";
    qInfo() << "   Remus compendium when the database is shipped with the build.";
    qInfo() << "   This CLI build does not expose a standalone catalog refresh command.";
    qInfo() << "";
    qInfo() << "";
    return 0;
}

// ── --import-dat ──────────────────────────────────────────────────────────────
int handleImportDatCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("import-dat"))
        return 0;

    qInfo() << "";
    qInfo() << "ℹ  Raw DAT import is no longer required.";
    qInfo() << "   Verification catalogs are bundled in the Remus compendium when present.";
    qInfo() << "   This CLI build does not expose a standalone catalog import or refresh command.";
    qInfo() << "";
    return 0;
}

// ── --remove-dat ─────────────────────────────────────────────────────────────
int handleRemoveDatCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("remove-dat"))
        return 0;

    qInfo() << "";
    qInfo() << "ℹ  Raw DAT management has been replaced by the Remus compendium.";
    qInfo() << "   There are no manually-imported DAT files to remove.";
    qInfo() << "   Verification catalog data lives inside the bundled compendium database.";
    qInfo() << "";
    return 0;
}

// ── --list-dats ──────────────────────────────────────────────────────────────
int handleListDatsCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("list-dats"))
        return 0;

    qInfo() << "";
    qInfo() << "ℹ  Raw DAT files are no longer used.";
    qInfo() << "   Verification catalogs are bundled in the Remus compendium when present.";
    qInfo() << "   This CLI build does not expose a dedicated catalog coverage report.";
    qInfo() << "";
    qInfo() << "";
    return 0;
}

// ── --dat-coverage ────────────────────────────────────────────────────────────
int handleDatCoverageCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("dat-coverage"))
        return 0;

    qInfo() << "";
    qInfo() << "=== DAT / Catalog Coverage Report ===";
    qInfo() << "";

    // ── Compendium catalog coverage ─────────────────────────────────────────
    const QString compendiumDir = findDataSubdir(QStringLiteral("compendium"));
    QSet<QString> compendiumSystemNames;
    int catalogEntryTotal = 0;

    if (!compendiumDir.isEmpty()) {
        const QString dbPath = compendiumDir + QStringLiteral("/remus_compendium.db");
        if (QFileInfo::exists(dbPath)) {
            const QString connName = QStringLiteral("dat_coverage_conn");
            {
                QSqlDatabase compDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
                compDb.setDatabaseName(dbPath);
                if (compDb.open()) {
                    QSqlQuery q(compDb);
                    q.exec(QStringLiteral("SELECT s.internal_name, COUNT(gs.game_id) AS cnt "
                                          "FROM systems s "
                                          "LEFT JOIN games gs ON gs.system_id = s.system_id "
                                          "GROUP BY s.system_id, s.internal_name"));
                    while (q.next()) {
                        const QString internalName = q.value(0).toString();
                        const int cnt = q.value(1).toInt();
                        compendiumSystemNames.insert(internalName);
                        catalogEntryTotal += cnt;
                    }
                }
            }
            QSqlDatabase::removeDatabase(connName);
        }
    }

    // ── Library DB coverage ─────────────────────────────────────────────────
    const QMap<QString, int> libCounts = ctx.db.getFileCountBySystem();

    // ── Cross-reference with all known systems ──────────────────────────────
    QStringList covered, uncoveredCompendium, uncoveredBoth;

    for (auto it = SYSTEMS.cbegin(); it != SYSTEMS.cend(); ++it) {
        const SystemDef &sys = it.value();
        const bool inCompendium = compendiumSystemNames.contains(sys.internalName);
        const bool inLibrary = libCounts.contains(sys.displayName) && libCounts.value(sys.displayName) > 0;

        if (inCompendium) {
            covered.append(sys.displayName);
        } else if (inLibrary) {
            uncoveredCompendium.append(sys.displayName);
        } else {
            uncoveredBoth.append(sys.displayName);
        }
    }

    std::sort(covered.begin(), covered.end());
    std::sort(uncoveredCompendium.begin(), uncoveredCompendium.end());
    std::sort(uncoveredBoth.begin(), uncoveredBoth.end());

    // ── Report ──────────────────────────────────────────────────────────────
    if (compendiumDir.isEmpty()) {
        qInfo() << "ℹ  No compendium database found.";
        qInfo() << "   Catalog coverage is unavailable without data/compendium/remus_compendium.db";
        qInfo() << "";
    } else {
        qInfo() << QString("Compendium: %1 total catalog entries across %2 systems")
                       .arg(catalogEntryTotal)
                       .arg(covered.size());
        qInfo() << "";
        qInfo() << QString("Systems with catalog data (%1):").arg(covered.size());
        for (const QString &name : std::as_const(covered)) {
            qInfo() << ("  ✔ " + name);
        }
    }

    if (!uncoveredCompendium.isEmpty()) {
        qInfo() << "";
        qInfo() << QString("Systems in your library but not in compendium (%1):").arg(uncoveredCompendium.size());
        for (const QString &name : std::as_const(uncoveredCompendium)) {
            qInfo() << ("  ! " + name);
        }
    }

    if (!uncoveredBoth.isEmpty()) {
        qInfo() << "";
        qInfo() << QString("Known systems with no coverage and no scanned files (%1):").arg(uncoveredBoth.size());
        for (const QString &name : std::as_const(uncoveredBoth)) {
            qInfo() << ("  - " + name);
        }
    }

    qInfo() << "";
    qInfo() << QString("Summary: %1 supported systems | %2 covered | %3 uncovered")
                   .arg(SYSTEMS.size())
                   .arg(covered.size())
                   .arg(uncoveredCompendium.size() + uncoveredBoth.size());
    qInfo() << "";
    return 0;
}

// ── --edit-metadata ──────────────────────────────────────────────────────────
int handleEditMetadataCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("edit-metadata"))
        return 0;

    const QString fileId = ctx.parser.value("edit-metadata");
    bool ok = false;
    int id = fileId.toInt(&ok);
    if (!ok || id <= 0) {
        qCritical() << "✗ Invalid file ID:" << fileId;
        return 1;
    }

    // Retrieve current file record
    FileRecord file = ctx.db.getFileById(id);
    if (file.id == 0) {
        qCritical() << "✗ File not found with ID:" << id;
        return 1;
    }

    // Find the matched game for this file
    MatchResult match = ctx.db.getMatchForFile(id);
    if (match.gameId == 0) {
        qCritical() << "✗ No metadata match found for file ID:" << id;
        qInfo() << "Run --match first to associate metadata with this file.";
        return 1;
    }

    // Collect edits from flags (empty string = keep existing per updateGame semantics)
    const QString title = ctx.parser.isSet("set-title") ? ctx.parser.value("set-title") : QString();
    const QString region = ctx.parser.isSet("set-region") ? ctx.parser.value("set-region") : QString();
    const QString genre = ctx.parser.isSet("set-genre") ? ctx.parser.value("set-genre") : QString();
    const QString developer = ctx.parser.isSet("set-developer") ? ctx.parser.value("set-developer") : QString();
    const QString publisher = ctx.parser.isSet("set-publisher") ? ctx.parser.value("set-publisher") : QString();

    if (title.isEmpty() && region.isEmpty() && genre.isEmpty() && developer.isEmpty() && publisher.isEmpty()) {
        qInfo() << "No metadata fields specified. Available flags:";
        qInfo() << "  --set-title, --set-region, --set-genre, --set-developer, --set-publisher";
        return 0;
    }

    // updateGame: only the fields the user specified are written; empty strings keep existing values
    if (!ctx.db.updateGame(match.gameId, publisher, developer,
            /*releaseDate*/ QString(),
            /*description*/ QString(), genre,
            /*players*/ QString(),
            /*rating*/ -1.0f, title, region)) {
        qCritical() << "✗ Failed to update game metadata";
        return 1;
    }

    qInfo() << "";
    qInfo() << "=== Metadata Updated ===";
    qInfo() << "File ID:" << id;
    qInfo() << "Game ID:" << match.gameId;
    if (!title.isEmpty())
        qInfo() << "  Title →" << title;
    if (!region.isEmpty())
        qInfo() << "  Region →" << region;
    if (!genre.isEmpty())
        qInfo() << "  Genre →" << genre;
    if (!developer.isEmpty())
        qInfo() << "  Developer →" << developer;
    if (!publisher.isEmpty())
        qInfo() << "  Publisher →" << publisher;
    return 0;
}
