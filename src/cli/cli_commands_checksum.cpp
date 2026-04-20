#include "cli_commands.h"
#include <QFileInfo>
#include "../core/hasher.h"
#include "../core/space_calculator.h"
#include "cli_logging.h"

using namespace Remus;

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
