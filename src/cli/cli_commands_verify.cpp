#include "cli_commands.h"
#include <QFileInfo>
#include "../core/hasher.h"
#include "../core/verification_engine.h"
#include "../core/space_calculator.h"
#include "cli_logging.h"

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

    if (verifier.importDat(datFile, systemName) <= 0) {
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
