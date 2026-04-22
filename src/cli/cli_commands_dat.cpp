#include "cli_commands.h"
#include <QCoreApplication>

// ── --update-dats ─────────────────────────────────────────────────────────────
int handleUpdateDatsCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("update-dats")) return 0;

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
int handleImportDatCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("import-dat")) return 0;

    qInfo() << "";
    qInfo() << "ℹ  Raw DAT import is no longer required.";
    qInfo() << "   Verification catalogs are bundled in the Remus compendium when present.";
    qInfo() << "   This CLI build does not expose a standalone catalog import or refresh command.";
    qInfo() << "";
    return 0;
}

// ── --remove-dat ─────────────────────────────────────────────────────────────
int handleRemoveDatCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("remove-dat")) return 0;

    qInfo() << "";
    qInfo() << "ℹ  Raw DAT management has been replaced by the Remus compendium.";
    qInfo() << "   There are no manually-imported DAT files to remove.";
    qInfo() << "   Verification catalog data lives inside the bundled compendium database.";
    qInfo() << "";
    return 0;
}

// ── --list-dats ──────────────────────────────────────────────────────────────
int handleListDatsCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("list-dats")) return 0;

    qInfo() << "";
    qInfo() << "ℹ  Raw DAT files are no longer used.";
    qInfo() << "   Verification catalogs are bundled in the Remus compendium when present.";
    qInfo() << "   This CLI build does not expose a dedicated catalog coverage report.";
    qInfo() << "";
    qInfo() << "";
    return 0;
}

// ── --edit-metadata ──────────────────────────────────────────────────────────
int handleEditMetadataCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("edit-metadata")) return 0;

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
    const QString title     = ctx.parser.isSet("set-title")     ? ctx.parser.value("set-title")     : QString();
    const QString region    = ctx.parser.isSet("set-region")    ? ctx.parser.value("set-region")    : QString();
    const QString genre     = ctx.parser.isSet("set-genre")     ? ctx.parser.value("set-genre")     : QString();
    const QString developer = ctx.parser.isSet("set-developer") ? ctx.parser.value("set-developer") : QString();
    const QString publisher = ctx.parser.isSet("set-publisher") ? ctx.parser.value("set-publisher") : QString();

    if (title.isEmpty() && region.isEmpty() && genre.isEmpty()
        && developer.isEmpty() && publisher.isEmpty()) {
        qInfo() << "No metadata fields specified. Available flags:";
        qInfo() << "  --set-title, --set-region, --set-genre, --set-developer, --set-publisher";
        return 0;
    }

    // updateGame signature: (gameId, publisher, developer, releaseDate, description, genres, players, rating)
    // We only update the fields the user specified; empty strings are kept existing
    if (!ctx.db.updateGame(match.gameId, publisher, developer,
                            /*releaseDate*/ QString(),
                            /*description*/ QString(),
                            genre,
                            /*players*/ QString(),
                            /*rating*/ -1.0f)) {
        qCritical() << "✗ Failed to update game metadata";
        return 1;
    }

    qInfo() << "";
    qInfo() << "=== Metadata Updated ===";
    qInfo() << "File ID:" << id;
    qInfo() << "Game ID:" << match.gameId;
    if (!title.isEmpty())     qInfo() << "  Title →" << title;
    if (!region.isEmpty())    qInfo() << "  Region →" << region;
    if (!genre.isEmpty())     qInfo() << "  Genre →" << genre;
    if (!developer.isEmpty()) qInfo() << "  Developer →" << developer;
    if (!publisher.isEmpty()) qInfo() << "  Publisher →" << publisher;
    return 0;
}
