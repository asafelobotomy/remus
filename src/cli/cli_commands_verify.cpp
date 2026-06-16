#include "cli_commands.h"
#include "cli_helpers.h"
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>
#include "../core/constants/files.h"
#include <QSqlQuery>
#include "../core/hasher.h"
#include "../core/verification_engine.h"
#include "../core/space_calculator.h"
#include "../core/dat_parser.h"
#include "../core/system_resolver.h"
#include "../metadata/clrmamepro_parser.h"
#include "cli_logging.h"

using namespace Remus;

namespace {

/// Detect whether file content is clrmamepro format (not XML).
bool isClrMameProFormat(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    const QString head = file.read(256).trimmed();
    file.close();
    return head.startsWith(QLatin1String("clrmamepro"))
        || (head.startsWith(QLatin1String("game")) && !head.startsWith(QLatin1Char('<')));
}

/// Convert clrmamepro entries to DatParseResult understood by VerificationEngine.
DatParseResult parseClrMameProAsDat(const QString &filePath) {
    DatParseResult result;

    QMap<QString, QString> hdr = ClrMameProParser::parseHeader(filePath);
    result.header.name = hdr.value(QStringLiteral("name"));
    result.header.description = hdr.value(QStringLiteral("description"));
    result.header.version = hdr.value(QStringLiteral("version"));

    const QList<ClrMameProEntry> entries = ClrMameProParser::parse(filePath);
    for (const ClrMameProEntry &ce : entries) {
        DatRomEntry entry;
        entry.gameName = ce.gameName;
        entry.description = ce.description;
        entry.romName = ce.romName;
        entry.size = ce.size;
        entry.crc32 = ce.crc32.toLower();
        entry.md5 = ce.md5.toLower();
        entry.sha1 = ce.sha1.toLower();
        entry.sha256 = ce.sha256.toLower();
        entry.serial = ce.serial;
        result.entries.append(entry);
    }

    result.entryCount = result.entries.size();
    result.success = result.entryCount > 0;
    if (!result.success)
        result.error = QStringLiteral("No entries found in clrmamepro DAT file");
    return result;
}

QString resolveSystemNameForDat(
    Database &db, const QString &datFilePath, const QString &headerName, const QString &fallbackName) {
    struct SystemRow {
        QString name;
        QString displayName;
    };

    QList<SystemRow> systems;
    {
        QSqlQuery q(db.database());
        q.prepare(QStringLiteral("SELECT name, display_name FROM systems"));
        if (q.exec()) {
            while (q.next()) {
                SystemRow row;
                row.name = q.value(0).toString();
                row.displayName = q.value(1).toString();
                systems.append(row);
            }
        }
    }

    auto normalize = [](QString value) {
        value = value.trimmed();
        // Remove parenthetical suffixes like "(Test)" from ad-hoc DAT names.
        value.remove(QRegularExpression(QStringLiteral("\\s*\\([^\\)]*\\)\\s*$")));
        value = value.trimmed();
        return value;
    };

    QStringList candidates;
    if (!headerName.isEmpty())
        candidates << normalize(headerName);
    if (!fallbackName.isEmpty())
        candidates << normalize(fallbackName);
    candidates.removeDuplicates();

    auto matchKnownSystem = [&systems](const QString &candidate) -> QString {
        if (candidate.isEmpty())
            return QString();

        // No-Intro style names often include vendor prefix, e.g. "Nintendo - Nintendo DS".
        QString trimmed = candidate;
        const int vendorSep = trimmed.indexOf(QStringLiteral(" - "));
        if (vendorSep >= 0 && vendorSep + 3 < trimmed.size()) {
            trimmed = trimmed.mid(vendorSep + 3).trimmed();
        }

        for (const SystemRow &row : systems) {
            if (row.name.compare(trimmed, Qt::CaseInsensitive) == 0
                || row.displayName.compare(trimmed, Qt::CaseInsensitive) == 0
                || row.name.compare(candidate, Qt::CaseInsensitive) == 0
                || row.displayName.compare(candidate, Qt::CaseInsensitive) == 0) {
                return row.name;
            }
        }

        // Fuzzy fallback: choose longest contained match to avoid short-token false positives.
        QString best;
        int bestLen = -1;
        const QString candidateLower = candidate.toLower();
        for (const SystemRow &row : systems) {
            const QString n = row.name.toLower();
            const QString d = row.displayName.toLower();
            if (candidateLower.contains(n) && row.name.size() > bestLen) {
                best = row.name;
                bestLen = row.name.size();
            }
            if (candidateLower.contains(d) && row.displayName.size() > bestLen) {
                best = row.name;
                bestLen = row.displayName.size();
            }
        }
        return best;
    };

    for (const QString &candidate : candidates) {
        const QString matched = matchKnownSystem(candidate);
        if (!matched.isEmpty()) {
            return matched;
        }
    }

    // Last resort for DAT imports in custom setups.
    return normalize(!headerName.isEmpty() ? headerName : fallbackName);
}

void printVerificationSummary(const VerificationSummary &summary) {
    qInfo() << "=== Verification Results ===";
    qInfo() << QString("Total files: %1").arg(summary.totalFiles);
    qInfo() << QString("✓ Verified: %1").arg(summary.verified);
    qInfo() << QString("⚠ Mismatched: %1").arg(summary.mismatched);
    qInfo() << QString("✗ Not in DAT: %1").arg(summary.notInDat);
    qInfo() << QString("? No hash: %1").arg(summary.noHash);
    qInfo() << "";
}

void printVerificationDetails(const QList<VerificationResult> &results) {
    if (results.isEmpty()) {
        return;
    }

    qInfo() << "Detailed Results:";
    qInfo() << "";

    int shown = 0;
    for (const VerificationResult &result : results) {
        if (result.status == VerificationStatus::Verified) {
            qInfo() << "✓" << result.filename << "- VERIFIED";
            qInfo() << "  Title:" << result.datDescription;
        } else if (result.status == VerificationStatus::Mismatch) {
            qWarning() << "✗" << result.filename << "- HASH MISMATCH";
            qWarning() << "  Expected:" << result.datHash;
            qWarning() << "  Got:     " << result.fileHash;
        } else if (result.status == VerificationStatus::NotInDat) {
            qInfo() << "?" << result.filename << "- NOT IN DAT";
        } else if (result.status == VerificationStatus::HashMissing) {
            qInfo() << "?" << result.filename << "- NO HASH (calculate with --hash)";
        }

        if (++shown >= 50) {
            qInfo() << "";
            qInfo() << "... and" << (results.size() - shown) << "more results";
            break;
        }
    }
}

int reportVerificationResults(
    CliContext &ctx, VerificationEngine &verifier, const QString &systemName, bool generateReport) {
    const QList<VerificationResult> results = verifier.verifyLibrary(systemName);
    const VerificationSummary summary = verifier.getLastSummary();

    printVerificationSummary(summary);
    printVerificationDetails(results);

    if (generateReport && ctx.parser.isSet("report-file")) {
        const QString reportPath = ctx.parser.value("report-file");
        if (verifier.exportReport(results, reportPath, "csv")) {
            qInfo() << "";
            qInfo() << "✓ CSV report saved to:" << reportPath;
        }
    }

    return 0;
}

bool importVerificationDat(CliContext &ctx, VerificationEngine &verifier, const QString &datFile, QString &systemName) {
    const QFileInfo datInfo(datFile);
    if (isClrMameProFormat(datFile)) {
        const DatParseResult parsed = parseClrMameProAsDat(datFile);
        systemName = resolveSystemNameForDat(ctx.db, datFile, parsed.header.name, datInfo.completeBaseName());
        return verifier.importDat(parsed, systemName) > 0;
    }

    DatParser parser;
    const DatParseResult parsed = parser.parse(datFile);
    systemName = resolveSystemNameForDat(ctx.db, datFile, parsed.header.name, datInfo.completeBaseName());
    return verifier.importDat(parsed, systemName) > 0;
}

} // anonymous namespace

int handleChecksumVerifyCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("checksum-verify"))
        return 0;

    const QString filePath = ctx.parser.value("checksum-verify");
    const QString expectedHash = ctx.parser.value("expected-hash");
    const QString hashType = ctx.parser.value("hash-type").toLower();

    qInfo() << "";
    qInfo() << "=== Verify Checksum ===";
    qInfo() << "File:" << filePath;
    qInfo() << "Hash Type:" << hashType;
    qInfo() << "Expected Hash:" << expectedHash;
    qInfo() << "";

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qCritical() << "✗ File not found:" << filePath;
        return 1;
    }

    const QString ext = QStringLiteral(".") + fileInfo.suffix().toLower();
    if (Constants::Files::isArchiveExtension(ext)) {
        qInfo() << "Note: hashing the archive container, not the inner ROM.";
        qInfo() << "      Extract the archive first if you need the inner file hash.";
        qInfo() << "";
    }

    Hasher hasher;
    HashResult result = hasher.calculateHashes(filePath, false, 0);
    QString calculatedHash;
    if (hashType == "md5")
        calculatedHash = result.md5.toLower();
    else if (hashType == "sha1")
        calculatedHash = result.sha1.toLower();
    else
        calculatedHash = result.crc32.toLower();

    qInfo() << "Calculated Hash:" << calculatedHash;

    if (calculatedHash.toLower() == expectedHash.toLower()) {
        qInfo() << "";
        qInfo() << "✓ HASH MATCH - File is valid!";
        qInfo() << "  File Size:" << SpaceCalculator::formatBytes(fileInfo.size());
    } else {
        qWarning() << "";
        qWarning() << "✗ HASH MISMATCH - File may be corrupted or modified!";
        qWarning() << "  Expected: " << expectedHash;
        qWarning() << "  Got:      " << calculatedHash;
        return 1;
    }
    return 0;
}

int handleVerifyCommand(CliContext &ctx) {
    if (!ctx.parser.isSet("verify"))
        return 0;

    const QString datFile = ctx.parser.value("verify");
    const bool generateReport = ctx.parser.isSet("verify-report");

    qInfo() << "";
    qInfo() << "=== Verify Files Against DAT ===";
    qInfo() << "DAT File:" << datFile;
    qInfo() << "";

    QFileInfo datInfo(datFile);
    if (!datInfo.exists()) {
        qCritical() << "✗ DAT file not found:" << datFile;
        return 1;
    }

    VerificationEngine verifier(&ctx.db);

    // Attach compendium as supplemental catalog when available
    const QString compendiumDir = findDataSubdir(QStringLiteral("compendium"));
    if (!compendiumDir.isEmpty()) {
        verifier.setCompendiumDb(compendiumDir + QStringLiteral("/remus_compendium.db"));
    }

    QString systemName;
    if (!importVerificationDat(ctx, verifier, datFile, systemName)) {
        qCritical() << "✗ Failed to import DAT file";
        return 1;
    }

    qInfo() << "✓ DAT file loaded successfully";
    qInfo() << "  System:" << systemName;
    qInfo() << "";

    return reportVerificationResults(ctx, verifier, systemName, generateReport);
}

int handlePatchDatCommand(CliContext &ctx) {
    const bool listRequested = ctx.parser.isSet("patch-dat-list");
    const bool importRequested = ctx.parser.isSet("patch-dat-import");
    const bool removeRequested = ctx.parser.isSet("patch-dat-remove");

    if (!listRequested && !importRequested && !removeRequested)
        return 0;

    VerificationEngine verifier(&ctx.db);

    // Attach compendium — patch catalogs are now bundled there
    const QString compendiumDir = findDataSubdir(QStringLiteral("compendium"));
    if (!compendiumDir.isEmpty()) {
        verifier.setCompendiumDb(compendiumDir + QStringLiteral("/remus_compendium.db"));
    }

    if (listRequested) {
        const auto patchDats = verifier.getImportedPatchDats();
        qInfo() << "";
        qInfo() << "=== Imported Patch Catalogs ===";

        if (patchDats.isEmpty()) {
            qInfo() << "No patch catalogs found in the bundled compendium.";
            return 0;
        }

        for (auto it = patchDats.cbegin(); it != patchDats.cend(); ++it) {
            qInfo() << it.key() << "-" << it.value().name << "(" << it.value().version << ")";
            if (!it.value().description.isEmpty()) {
                qInfo() << "  " << it.value().description;
            }
        }
        return 0;
    }

    if (importRequested) {
        qInfo() << "";
        qInfo() << "ℹ  Patch catalogs are now bundled in the Remus compendium.";
        qInfo() << "   Manual patch-DAT import is no longer required.";
        qInfo() << "   This CLI build does not expose a standalone patch catalog refresh command.";
        qInfo() << "";
        return 0;
    }

    // --patch-dat-remove
    qInfo() << "";
    qInfo() << "ℹ  Patch catalogs are now bundled in the Remus compendium.";
    qInfo() << "   Manual patch-DAT removal is no longer required.";
    qInfo() << "";
    return 0;
}
