#include "cli_commands.h"
#include "cli_helpers.h"
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include "../metadata/provider_orchestrator.h"
#include "../core/constants/constants.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

int handleMatchCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("match") && !ctx.processRequested) return 0;

    qInfo() << "";
    qInfo() << "=== Intelligent Metadata Matching (M3) ===";
    qInfo() << "";

    auto orchestrator = buildOrchestrator(ctx.parser);

    QObject::connect(orchestrator.get(), &ProviderOrchestrator::tryingProvider,
                     [](const QString &name, const QString &method) {
        qInfo() << "  [TRYING]" << name << "(" << method << ")";
    });
    QObject::connect(orchestrator.get(), &ProviderOrchestrator::providerSucceeded,
                     [](const QString &name, const QString &method) {
        qInfo() << "  [SUCCESS]" << name << "matched via" << method;
    });
    QObject::connect(orchestrator.get(), &ProviderOrchestrator::providerFailed,
                     [](const QString &name, const QString &error) {
        qInfo() << "  [FAILED]" << name << "-" << error;
    });

    QList<FileRecord> files = getHashedFiles(ctx.db);
    int minConfidence = ctx.parser.value("min-confidence").toInt();

    qInfo() << "Matching" << files.size() << "files with minimum confidence:" << minConfidence << "%";
    qInfo() << "Provider fallback order:";
    for (const QString &p : orchestrator->getEnabledProviders()) {
        const QString hashSupport = orchestrator->providerSupportsHash(p) ? "✓ hash" : "✗ name only";
        qInfo() << "  -" << p << "(" << hashSupport << ")";
    }
    qInfo() << "";

    int matched = 0, failed = 0;

    for (const FileRecord &file : files) {
        if (ctx.db.getMatchForFile(file.id).matchId != 0) continue;

        qInfo() << "Matching:" << file.filename;

        GameMetadata metadata = orchestrator->searchWithFallback(
            selectBestHash(file), file.filename, "",
            file.crc32, file.md5, file.sha1);

        if (!metadata.title.isEmpty()) {
            const int confidence = metadata.matchScore > 0
                ? static_cast<int>(metadata.matchScore * 100) : 0;

            if (confidence >= minConfidence) {
                int gameId = persistMetadata(ctx.db, file, metadata);
                qInfo() << "  ✓ MATCHED:" << metadata.title << "(" << confidence << "% confidence)";
                qInfo() << "    Provider:" << metadata.providerId;
                qInfo() << "    Method:"   << metadata.matchMethod;
                qInfo() << "    System:"   << metadata.system;
                qInfo() << "    Game ID:"  << gameId;
                matched++;
            } else {
                qInfo() << "  ⚠ Low confidence:" << confidence
                        << "% (threshold:" << minConfidence << "%)";
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
    qInfo() << "Failed:"  << failed;
    if (matched + failed > 0) {
        qInfo() << "Success rate:"
                << QString::number((matched * 100.0) / (matched + failed), 'f', 1) + "%";
    }
    return 0;
}

int handleMatchReportCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("match-report")) return 0;

    qInfo() << "";
    qInfo() << "=== Matching Report with Confidence Scores ===";
    qInfo() << "";

    auto orchestrator  = buildOrchestrator(ctx.parser);
    QList<FileRecord> files = getHashedFiles(ctx.db);
    int minConfidence  = ctx.parser.value("min-confidence").toInt();

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
    outStream << QString("Minimum confidence threshold: %1%\n\n").arg(minConfidence);

    outStream << "┌────────────┬──────────────────────────────┬──────────┬──────────┬──────────────────────┐\n";
    outStream << "│ ID         │ Filename                     │ Conf %   │ Method   │ Title                │\n";
    outStream << "├────────────┼──────────────────────────────┼──────────┼──────────┼──────────────────────┤\n";

    for (const FileRecord &file : files) {
        GameMetadata metadata = orchestrator->searchWithFallback(
            selectBestHash(file), file.filename, "",
            file.crc32, file.md5, file.sha1);

        const int confidence = metadata.matchScore > 0
            ? static_cast<int>(metadata.matchScore * 100) : 0;
        const QString method = metadata.matchMethod.isEmpty() ? "N/A" : metadata.matchMethod;
        const QString title  = metadata.title.isEmpty()       ? "No match" : metadata.title;

        QString indicator;
        if      (confidence >= 90) indicator = "✓✓✓";
        else if (confidence >= 70) indicator = "✓✓";
        else if (confidence >= 50) indicator = "✓";
        else                       indicator = "✗";

        outStream << QString("│ %1 │ %2 │ %3 %4 │ %5 │ %6 │\n")
            .arg(QString::number(file.id).leftJustified(10))
            .arg(file.filename.left(28).leftJustified(28))
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
