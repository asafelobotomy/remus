#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFile>
#include <QMessageLogContext>
#include <QSet>
#include <QStringList>
#include <QTextStream>
#include <memory>
#include "cli_commands.h"
#include "cli_helpers.h"
#include "cli_options.h"
#include "../core/constants/constants.h"
#include "cli_logging.h"

using namespace Remus;
using namespace Remus::Constants;

static bool hasFlag(const QStringList &args, const QString &flag)
{
    return args.contains(flag);
}

static QString normalizeOptionToken(const QString &arg)
{
    if (!arg.startsWith('-')) {
        return {};
    }

    int start = 0;
    while (start < arg.size() && arg.at(start) == '-') {
        ++start;
    }

    if (start >= arg.size()) {
        return {};
    }

    return arg.mid(start).section('=', 0, 0);
}

static bool hasAnyAction(const QStringList &args, const QSet<QString> &actionOptions)
{
    for (const QString &arg : args) {
        const QString token = normalizeOptionToken(arg);
        if (!token.isEmpty() && actionOptions.contains(token)) {
            return true;
        }
    }
    return false;
}

static void printBanner()
{
    qInfo() << "╔════════════════════════════════════════╗";
    qInfo() << "║  Remus - Retro Game Library Manager   ║";
    qInfo() << "╚════════════════════════════════════════╝";
    qInfo() << "";
}

static void machineReadableMessageHandler(QtMsgType type,
                                          const QMessageLogContext &context,
                                          const QString &msg)
{
    Q_UNUSED(context);

    if (type == QtDebugMsg || type == QtInfoMsg) {
        return;
    }

    QTextStream stream(stderr);
    stream << msg << Qt::endl;

    if (type == QtFatalMsg) {
        abort();
    }
}

// Global file pointer for --log-file tee output (non-JSON mode only).
static std::unique_ptr<QFile> g_logFile;

static void logFileTeeMessageHandler(QtMsgType type,
                                     const QMessageLogContext &context,
                                     const QString &msg)
{
    Q_UNUSED(context);

    if (type == QtDebugMsg || type == QtInfoMsg) {
        QTextStream(stdout) << msg << Qt::endl;
    } else {
        QTextStream(stderr) << msg << Qt::endl;
    }

    if (g_logFile && g_logFile->isOpen()) {
        QTextStream fileStream(g_logFile.get());
        fileStream << msg << Qt::endl;
    }

    if (type == QtFatalMsg) {
        abort();
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(Constants::Cli::APPLICATION_NAME);
    QCoreApplication::setOrganizationName(Constants::SETTINGS_ORGANIZATION);
    QCoreApplication::setApplicationVersion(Constants::APP_VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription("Remus CLI - Scan and catalog retro game ROMs");
    parser.addHelpOption();
    parser.addVersionOption();

    QSet<QString> actionOptions = {
        QStringLiteral("help"),
        QStringLiteral("h"),
        QStringLiteral("version")
    };

    registerAllOptions(parser, actionOptions);

    QStringList activeArgs = app.arguments();
    const bool jsonRequested = hasFlag(activeArgs, "--" + Constants::Cli::Options::JSON) || hasFlag(activeArgs, "--" + Constants::Cli::Options::MOD_JSON);
    const bool actionsProvided   = hasAnyAction(activeArgs, actionOptions);

    if (jsonRequested) {
        qInstallMessageHandler(machineReadableMessageHandler);
    }

    if (!jsonRequested) {
        printBanner();
    }

    parser.process(activeArgs);

    if (!jsonRequested && parser.isSet(QStringLiteral("log-file"))) {
        const QString logPath = parser.value(QStringLiteral("log-file"));
        g_logFile = std::make_unique<QFile>(logPath);
        if (!g_logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
            qWarning() << "remus: could not open log file for writing:" << logPath;
            g_logFile.reset();
        } else {
            qInstallMessageHandler(logFileTeeMessageHandler);
        }
    }

    if (!actionsProvided) {
        parser.showHelp(0);
    }

    Database db;
    if (!db.initialize(parser.value("db"))) {
        qCritical() << "Failed to initialize database";
        return 1;
    }

    SystemDetector detector;

    CliContext ctx{parser, db, detector,
                   /*dryRunAll*/        parser.isSet(Constants::Cli::Options::DRY_RUN_ALL),
                   /*processRequested*/ parser.isSet("process") || parser.isSet("library"),
                   /*processHandled*/   false,
                   /*processSystemIdFilter*/ -1,
                   /*processFileScopeIds*/ {},
                   /*presetBundleFormat*/  {},
                   /*presetDiscFormat*/    {},
                   /*presetFolderNaming*/  {},
                   /*presetDisplayName*/   {},
                   /*processArtworkCacheDir*/ {},
                   /*processSourcePath*/  {},
                   /*processOutputPath*/  {}};

    // Resolve --process-preset into concrete overrides
    if (parser.isSet("process-preset")) {
        const QString presetKey = parser.value("process-preset").trimmed().toLower();
        if (Constants::Cli::PROCESS_PRESETS.contains(presetKey)) {
            const auto &preset = Constants::Cli::PROCESS_PRESETS.value(presetKey);
            ctx.presetBundleFormat  = QString::fromLatin1(preset.bundleFormat);
            ctx.presetDiscFormat    = QString::fromLatin1(preset.discFormat);
            ctx.presetFolderNaming  = QString::fromLatin1(preset.folderNaming);
            ctx.presetDisplayName   = QString::fromLatin1(preset.displayName);
        } else {
            qCritical() << "Unknown preset:" << presetKey;
            qInfo() << "Available presets:" << Constants::Cli::PROCESS_PRESET_NAMES.join(", ");
            return 1;
        }
    }

    // Resolve canonical source/output paths from --process or --library
    if (parser.isSet("process")) {
        ctx.processSourcePath = parser.value("process");
        if (parser.isSet("process-output"))
            ctx.processOutputPath = parser.value("process-output");
        else if (parser.isSet("bundle"))
            ctx.processOutputPath = parser.value("bundle");
    }
    if (parser.isSet("library")) {
        ctx.processSourcePath = parser.value("library");
        ctx.processOutputPath = parser.value("output");
        // Apply consumer-friendly defaults when not already set by a preset or explicit flags
        if (ctx.presetBundleFormat.isEmpty())  ctx.presetBundleFormat  = QStringLiteral("7z");
        if (ctx.presetDiscFormat.isEmpty())    ctx.presetDiscFormat    = QStringLiteral("chd");
        if (ctx.presetFolderNaming.isEmpty())  ctx.presetFolderNaming  = QStringLiteral("default");
        if (ctx.presetDisplayName.isEmpty())   ctx.presetDisplayName   = QStringLiteral("library");
    }

    if (int rc = handleStatsCommand(ctx))          return rc;
    if (int rc = handleInfoCommand(ctx))           return rc;
    if (int rc = handleInspectCommands(ctx))       return rc;
    if (int rc = handleScanCommand(ctx))           return rc;
    if (int rc = handleListCommand(ctx))           return rc;
    if (int rc = handleHashAllCommand(ctx))        return rc;
    if (int rc = handleReclassifyIsoCommand(ctx))  return rc;
    if (int rc = handleMetadataCommand(ctx))       return rc;
    if (int rc = handleSearchCommand(ctx))         return rc;
    if (int rc = handleMatchCommand(ctx))          return rc;
    if (int rc = handleEnrichCommand(ctx))         return rc;
    if (int rc = handleMatchReportCommand(ctx))    return rc;
    if (int rc = handleChecksumVerifyCommand(ctx)) return rc;
    if (int rc = handleVerifyCommand(ctx))         return rc;
    if (int rc = handlePatchDatCommand(ctx))       return rc;
    if (int rc = handleArtworkCommand(ctx))        return rc;
    if (int rc = handleBundleCommand(ctx))         return rc;
    if (int rc = handleOrganizeCommand(ctx))       return rc;
    if (int rc = handleGenerateM3uCommand(ctx))    return rc;
    if (int rc = handleConvertChdCommand(ctx))     return rc;
    if (int rc = handleChdExtractCommand(ctx))     return rc;
    if (int rc = handleChdVerifyCommand(ctx))      return rc;
    if (int rc = handleChdInfoCommand(ctx))        return rc;
    if (int rc = handleExtractArchiveCommand(ctx)) return rc;
    if (int rc = handleSpaceReportCommand(ctx))    return rc;
    if (int rc = handleConvertRvzCommand(ctx))     return rc;
    if (int rc = handleRvzExtractCommand(ctx))     return rc;
    if (int rc = handleRvzVerifyCommand(ctx))      return rc;
    if (int rc = handleConvertCsoCommand(ctx))     return rc;
    if (int rc = handleCsoExtractCommand(ctx))     return rc;
    if (int rc = handleConvertWbfsCommand(ctx))    return rc;
    if (int rc = handleWbfsExtractCommand(ctx))    return rc;
    if (int rc = handleExportPBPCommand(ctx))      return rc;
    if (int rc = handleExportCommand(ctx))         return rc;
    if (int rc = handlePatchCommands(ctx))         return rc;
    if (int rc = handleModCommands(ctx))           return rc;
    if (int rc = handleModCatalogBuildCommand(ctx)) return rc;
    if (int rc = handleBuildCompendiumCommand(ctx)) return rc;
    if (int rc = handleEnrichCompendiumCommand(ctx)) return rc;
    if (int rc = handleUpdateDatsCommand(ctx))     return rc;
    if (int rc = handleImportDatCommand(ctx))      return rc;
    if (int rc = handleRemoveDatCommand(ctx))      return rc;
    if (int rc = handleListDatsCommand(ctx))       return rc;
    if (int rc = handleDatCoverageCommand(ctx))    return rc;
    if (int rc = handleEditMetadataCommand(ctx))   return rc;

    if (!jsonRequested) {
        qInfo() << "";
        qInfo() << "Done!";
    }
    return 0;
}
