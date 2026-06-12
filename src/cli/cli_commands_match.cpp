#include "cli_commands.h"
#include "cli_helpers.h"
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QtConcurrent>
#include "../metadata/provider_orchestrator.h"
#include "../core/constants/constants.h"
#include "../core/disc_magic_detector.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

namespace {

// Carries a file record together with its pre-detected disc serial.
struct MatchTask {
    FileRecord file;
    QString discSerial;
};

// Thread-safe: all DiscMagicDetector methods are static and create their own
// worker-local ArchiveExtractor instances.  No shared mutable state.
MatchTask buildMatchTask(const FileRecord &file) {
    MatchTask task;
    task.file = file;
    if (!DiscMagicDetector::isDiscImageExtension(file.extension))
        return task;

    DiscHeaderInfo discInfo;
    if (file.isCompressed && !file.archivePath.isEmpty()) {
        const QString memberPath = file.archiveInternalPath.isEmpty() ? file.filename : file.archiveInternalPath;
        discInfo = DiscMagicDetector::detectFromArchive(file.archivePath, memberPath, file.fileSize);
    } else {
        discInfo = DiscMagicDetector::detect(file.currentPath);
        if (!discInfo.detected || discInfo.serial.isEmpty()) {
            // CDI files: fall back to the Dreamcast deep scanner.
            DiscHeaderInfo dcInfo = DiscMagicDetector::extractDreamcastHeader(file.currentPath);
            if (dcInfo.detected && !dcInfo.serial.isEmpty())
                discInfo = dcInfo;
        }
    }

    if (discInfo.detected && !discInfo.serial.isEmpty())
        task.discSerial = discInfo.serial;
    return task;
}

} // namespace

int handleMatchCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("match") && !ctx.processRequested)
        return 0;
    if (ctx.processRequested && ctx.processHandled)
        return 0;

    qInfo() << "";
    qInfo() << "=== Intelligent Metadata Matching (M3) ===";
    qInfo() << "";

    auto orchestrator = buildOrchestrator(ctx.parser, &ctx.db);

    QObject::connect(orchestrator.get(), &ProviderOrchestrator::tryingProvider,
        [](const QString &name, const QString &method) { qInfo() << "  [TRYING]" << name << "(" << method << ")"; });
    QObject::connect(
        orchestrator.get(), &ProviderOrchestrator::providerSucceeded, [](const QString &name, const QString &method) {
            qInfo() << "  [SUCCESS]" << name << "matched via" << method;
        });
    QObject::connect(orchestrator.get(), &ProviderOrchestrator::providerFailed,
        [](const QString &name, const QString &error) { qInfo() << "  [FAILED]" << name << "-" << error; });

    QList<FileRecord> files = getHashedFiles(ctx.db, ctx.processFileScopeIds);
    int minConfidence = ctx.parser.value(Cli::Options::MIN_CONFIDENCE).toInt();

    qInfo() << "Matching" << files.size() << "files with minimum confidence:" << minConfidence << "%";
    qInfo() << "Provider fallback order:";
    for (const QString &p : orchestrator->getEnabledProviders()) {
        const QString hashSupport = orchestrator->providerSupportsHash(p) ? "✓ hash" : "✗ name only";
        qInfo() << "  -" << p << "(" << hashSupport << ")";
    }
    qInfo() << "";

    // Pre-filter: collect files that still need matching (serial DB checks must
    // stay on the main thread — the Database connection is not thread-safe).
    QList<FileRecord> pendingFiles;
    for (const FileRecord &file : files) {
        if (!fileMatchesSystemFilter(file, ctx.processSystemIdFilter))
            continue;
        if (ctx.db.getMatchForFile(file.id).matchId != 0)
            continue;
        pendingFiles.append(file);
    }
    qInfo() << "Files pending match:" << pendingFiles.size();
    qInfo() << "";

    // Phase 1 — Parallel disc serial detection.
    // DiscMagicDetector methods are static and worker-local; safe for concurrent
    // use.  HttpMetadataProvider instances (QNAM thread affinity) are NOT touched
    // here — network provider calls remain serial in Phase 2.
    const QList<MatchTask> tasks
        = QtConcurrent::blockingMapped(pendingFiles, [](const FileRecord &f) { return buildMatchTask(f); });

    // Phase 2 — Serial provider matching and DB writes.
    // QNetworkAccessManager inside HttpMetadataProvider has thread affinity to
    // its owning (main) thread; searchWithFallback() and persistMetadata() must
    // not be dispatched to worker threads.
    int matched = 0, failed = 0;

    for (const MatchTask &task : tasks) {
        const FileRecord &file = task.file;
        const QString displayName = getMatchingDisplayName(file);
        const QString systemName = getMatchingSystemName(file);

        qInfo() << "Matching:" << displayName;
        if (!task.discSerial.isEmpty())
            qInfo() << "  Disc serial:" << task.discSerial;

        GameMetadata         metadata = orchestrator->searchWithFallback(
            selectBestHash(file), displayName, systemName, file.crc32, file.md5, file.sha1, task.discSerial,
            file.fileSize, file.raMd5);

        if (!metadata.title.isEmpty()) {
            const int confidence = metadata.matchScore > 0 ? static_cast<int>(metadata.matchScore * 100) : 0;

            if (confidence >= minConfidence) {
                int gameId = persistMetadata(ctx.db, file, metadata);
                qInfo() << "  ✓ MATCHED:" << metadata.title << "(" << confidence << "% confidence)";
                qInfo() << "    Provider:" << metadata.providerId;
                qInfo() << "    Method:" << metadata.matchMethod;
                qInfo() << "    System:" << metadata.system;
                qInfo() << "    Game ID:" << gameId;
                matched++;
            } else {
                qInfo() << "  ⚠ Low confidence:" << confidence << "% (threshold:" << minConfidence << "%)";
                failed++;
            }
        } else {
            qInfo() << "  ✗ No match found";
            failed++;
        }
        qInfo() << "";
    }

    qInfo() << "=== Matching Complete ===";
    qInfo() << "Matched:" << matched;
    qInfo() << "Failed:" << failed;
    if (matched + failed > 0) {
        qInfo() << "Success rate:" << QString::number((matched * 100.0) / (matched + failed), 'f', 1) + "%";
    }
    return 0;
}

int handleMatchReportCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("match-report"))
        return 0;

    qInfo() << "";
    qInfo() << "=== Matching Report with Confidence Scores ===";
    qInfo() << "";

    QList<FileRecord> files = getHashedFiles(ctx.db, ctx.processFileScopeIds);
    QMap<int, Database::MatchResult> matches = ctx.db.getAllMatches();

    QFile reportFile;
    QTextStream outStream(stdout);

    if (ctx.parser.isSet("report-file")) {
        reportFile.setFileName(ctx.parser.value("report-file"));
        if (!reportFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qCritical() << "Failed to open report file:" << ctx.parser.value("report-file");
            return 1;
        }
        outStream.setDevice(&reportFile);
    }

    outStream << "\n=== Matching Confidence Report ===\n";
    outStream << QString("Generated: %1\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    outStream << QString("Total files: %1\n").arg(files.size());
    outStream << QString("Matched files: %1\n\n").arg(matches.size());

    outStream << "┌────────────┬──────────────────────────────┬──────────┬──────────┬──────────────────────┐\n";
    outStream << "│ ID         │ Filename                     │ Conf %   │ Method   │ Title                │\n";
    outStream << "├────────────┼──────────────────────────────┼──────────┼──────────┼──────────────────────┤\n";

    for (const FileRecord &file : files) {
        const QString displayName = getMatchingDisplayName(file);

        int confidence = 0;
        QString method = "N/A";
        QString title = "No match";

        if (matches.contains(file.id)) {
            const auto &match = matches[file.id];
            confidence = static_cast<int>(match.confidence);
            method = match.matchMethod.isEmpty() ? "N/A" : match.matchMethod;
            title = match.gameTitle.isEmpty() ? "No match" : match.gameTitle;
        }

        QString indicator;
        if (confidence >= 90)
            indicator = "✓✓✓";
        else if (confidence >= 70)
            indicator = "✓✓";
        else if (confidence >= 50)
            indicator = "✓";
        else
            indicator = "✗";

        outStream << QString("│ %1 │ %2 │ %3 %4 │ %5 │ %6 │\n")
                         .arg(QString::number(file.id).leftJustified(10))
                         .arg(displayName.left(28).leftJustified(28))
                         .arg(QString::number(confidence).rightJustified(4))
                         .arg(indicator.rightJustified(3))
                         .arg(method.leftJustified(8))
                         .arg(title.left(19).leftJustified(19));
    }

    outStream << "└────────────┴──────────────────────────────┴──────────┴──────────┴──────────────────────┘\n";
    outStream << "\nLegend:\n";
    outStream << "  ✓✓✓ = Excellent confidence (≥90%)\n";
    outStream << "  ✓✓  = Good confidence (70-89%)\n";
    outStream << "  ✓   = Fair confidence (50-69%)\n";
    outStream << "  ✗   = Low confidence (<50%)\n";

    if (ctx.parser.isSet("report-file")) {
        reportFile.close();
        qInfo() << "✓ Report saved to:" << ctx.parser.value("report-file");
    }
    return 0;
}
