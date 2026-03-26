#include "cli_commands.h"
#include <QFileInfo>
#include <QFile>
#include "../core/hasher.h"
#include "../core/verification_engine.h"
#include "../core/space_calculator.h"
#include "../core/dat_parser.h"
#include "../metadata/clrmamepro_parser.h"
#include "cli_logging.h"

using namespace Remus;

namespace {

/// Detect whether file content is clrmamepro format (not XML).
bool isClrMameProFormat(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    const QString head = file.read(256).trimmed();
    file.close();
    return head.startsWith(QLatin1String("clrmamepro")) ||
           (head.startsWith(QLatin1String("game")) && !head.startsWith(QLatin1Char('<')));
}

/// Convert clrmamepro entries to DatParseResult understood by VerificationEngine.
DatParseResult parseClrMameProAsDat(const QString &filePath)
{
    DatParseResult result;

    QMap<QString, QString> hdr = ClrMameProParser::parseHeader(filePath);
    result.header.name        = hdr.value(QStringLiteral("name"));
    result.header.description = hdr.value(QStringLiteral("description"));
    result.header.version     = hdr.value(QStringLiteral("version"));

    const QList<ClrMameProEntry> entries = ClrMameProParser::parse(filePath);
    for (const ClrMameProEntry &ce : entries) {
        DatRomEntry entry;
        entry.gameName    = ce.gameName;
        entry.description = ce.description;
        entry.romName     = ce.romName;
        entry.size        = ce.size;
        entry.crc32       = ce.crc32.toLower();
        entry.md5         = ce.md5.toLower();
        entry.sha1        = ce.sha1.toLower();
        entry.serial      = ce.serial;
        result.entries.append(entry);
    }

    result.entryCount = result.entries.size();
    result.success    = result.entryCount > 0;
    if (!result.success)
        result.error = QStringLiteral("No entries found in clrmamepro DAT file");
    return result;
}

} // anonymous namespace

int handleChecksumVerifyCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("checksum-verify")) return 0;

    const QString filePath     = ctx.parser.value("checksum-verify");
    const QString expectedHash = ctx.parser.value("expected-hash");
    const QString hashType     = ctx.parser.value("hash-type").toLower();

    qInfo() << "";
    qInfo() << "=== Verify Checksum ===";
    qInfo() << "File:"          << filePath;
    qInfo() << "Hash Type:"     << hashType;
    qInfo() << "Expected Hash:" << expectedHash;
    qInfo() << "";

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        qCritical() << "✗ File not found:" << filePath;
        return 1;
    }

    Hasher hasher;
    HashResult result = hasher.calculateHashes(filePath, false, 0);
    QString calculatedHash;
    if      (hashType == "md5")  calculatedHash = result.md5.toLower();
    else if (hashType == "sha1") calculatedHash = result.sha1.toLower();
    else                          calculatedHash = result.crc32.toLower();

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

int handleVerifyCommand(CliContext &ctx)
{
    if (!ctx.parser.isSet("verify")) return 0;

    const QString datFile      = ctx.parser.value("verify");
    const bool generateReport  = ctx.parser.isSet("verify-report");

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

    QString systemName = ctx.detector.detectSystem("", datFile);
    if (systemName.isEmpty()) systemName = datInfo.completeBaseName();

    int importCount = 0;
    if (isClrMameProFormat(datFile)) {
        DatParseResult parsed = parseClrMameProAsDat(datFile);
        importCount = verifier.importDat(parsed, systemName);
    } else {
        importCount = verifier.importDat(datFile, systemName);
    }

    if (importCount <= 0) {
        qCritical() << "✗ Failed to import DAT file";
        return 1;
    }

    qInfo() << "✓ DAT file loaded successfully";
    qInfo() << "  System:" << systemName;
    qInfo() << "";

    QList<VerificationResult> results = verifier.verifyLibrary(systemName);
    VerificationSummary summary        = verifier.getLastSummary();

    qInfo() << "=== Verification Results ===";
    qInfo() << QString("Total files: %1").arg(summary.totalFiles);
    qInfo() << QString("✓ Verified: %1").arg(summary.verified);
    qInfo() << QString("⚠ Mismatched: %1").arg(summary.mismatched);
    qInfo() << QString("✗ Not in DAT: %1").arg(summary.notInDat);
    qInfo() << QString("? No hash: %1").arg(summary.noHash);
    qInfo() << "";

    if (!results.isEmpty()) {
        qInfo() << "Detailed Results:";
        qInfo() << "";
        int shown = 0;
        for (const VerificationResult &r : results) {
            if      (r.status == VerificationStatus::Verified)    {
                qInfo() << "✓" << r.filename << "- VERIFIED";
                qInfo() << "  Title:" << r.datDescription;
            } else if (r.status == VerificationStatus::Mismatch) {
                qWarning() << "✗" << r.filename << "- HASH MISMATCH";
                qWarning() << "  Expected:" << r.datHash;
                qWarning() << "  Got:     " << r.fileHash;
            } else if (r.status == VerificationStatus::NotInDat) {
                qInfo() << "?" << r.filename << "- NOT IN DAT";
            } else if (r.status == VerificationStatus::HashMissing) {
                qInfo() << "?" << r.filename << "- NO HASH (calculate with --hash)";
            }
            if (++shown >= 50) {
                qInfo() << "";
                qInfo() << "... and" << (results.size() - shown) << "more results";
                break;
            }
        }
    }

    if (generateReport && ctx.parser.isSet("report-file")) {
        const QString reportPath = ctx.parser.value("report-file");
        if (verifier.exportReport(results, reportPath, "csv")) {
            qInfo() << "";
            qInfo() << "✓ CSV report saved to:" << reportPath;
        }
    }
    return 0;
}

int handlePatchDatCommand(CliContext &ctx)
{
    const bool listRequested = ctx.parser.isSet("patch-dat-list");
    const bool importRequested = ctx.parser.isSet("patch-dat-import");
    const bool removeRequested = ctx.parser.isSet("patch-dat-remove");

    if (!listRequested && !importRequested && !removeRequested) return 0;

    VerificationEngine verifier(&ctx.db);

    if (listRequested) {
        const auto patchDats = verifier.getImportedPatchDats();
        qInfo() << "";
        qInfo() << "=== Imported Patch Catalogs ===";

        if (patchDats.isEmpty()) {
            qInfo() << "No patch catalogs imported.";
            return 0;
        }

        for (auto it = patchDats.cbegin(); it != patchDats.cend(); ++it) {
            qInfo() << it.key() << "-" << it.value().name
                    << "(" << it.value().version << ")";
            if (!it.value().description.isEmpty()) {
                qInfo() << "  " << it.value().description;
            }
        }
        return 0;
    }

    if (importRequested) {
        const QString datFile = ctx.parser.value("patch-dat-import");
        QFileInfo datInfo(datFile);
        if (!datInfo.exists()) {
            qCritical() << "✗ Patch DAT file not found:" << datFile;
            return 1;
        }

        QString systemName = ctx.parser.value("patch-dat-system");
        if (systemName.isEmpty()) {
            systemName = ctx.detector.detectSystem(QString(), datFile);
        }
        if (systemName.isEmpty()) {
            systemName = datInfo.completeBaseName();
        }

        const int count = verifier.importPatchDat(datFile, systemName);
        if (count <= 0) {
            qCritical() << "✗ Failed to import patch DAT file";
            return 1;
        }

        qInfo() << "";
        qInfo() << "=== Patch Catalog Imported ===";
        qInfo() << "System:" << systemName;
        qInfo() << "Entries:" << count;
        qInfo() << "File:" << datFile;
        return 0;
    }

    const QString systemName = ctx.parser.value("patch-dat-remove");
    if (systemName.isEmpty()) {
        qCritical() << "✗ --patch-dat-remove requires a system name";
        return 1;
    }

    if (!verifier.removePatchDat(systemName)) {
        qCritical() << "✗ Failed to remove patch DAT for system:" << systemName;
        return 1;
    }

    qInfo() << "Removed patch DAT for" << systemName;
    return 0;
}
