#include "match_utils.h"

#include <QFileInfo>
#include <QSet>

#include "constants/constants.h"
#include "matching_engine.h"
#include "patched_rom_parser.h"
#include "verification_hash_matcher.h"

namespace Remus {

QString selectBestMatchHash(const FileRecord &file) {
    if (Constants::Systems::SYSTEMS.contains(file.systemId)) {
        const Constants::Systems::SystemDef &systemDef = Constants::Systems::SYSTEMS[file.systemId];
        const QString preferred = systemDef.preferredHash.toLower();

        if (preferred == "md5" && !file.md5.isEmpty())
            return file.md5;
        if (preferred == "sha1" && !file.sha1.isEmpty())
            return file.sha1;
        if (preferred == "crc32" && !file.crc32.isEmpty())
            return file.crc32;
    }

    if (!file.crc32.isEmpty())
        return file.crc32;
    if (!file.sha1.isEmpty())
        return file.sha1;
    if (!file.md5.isEmpty())
        return file.md5;
    return QString();
}

QString selectContentSha1(const FileRecord &file) {
    if (!file.chdSha1.isEmpty())
        return file.chdSha1.trimmed().toLower();
    if (!file.rvzSha1.isEmpty())
        return file.rvzSha1.trimmed().toLower();
    return QString();
}

QString deriveMatchingDisplayName(const FileRecord &file) {
    if (!file.baseTitle.isEmpty()) {
        return file.baseTitle;
    }

    const auto usePatchedBaseTitle = [](const QString &name) {
        const PatchedRomInfo info = PatchedRomParser::parse(name);
        const bool derivedPatchedVariant = info.isPatched || Constants::FileTypes::isPatchedVariant(info.fileType);
        return derivedPatchedVariant && !info.baseTitle.isEmpty() ? info.baseTitle : QString();
    };

    if (file.isCompressed) {
        const QString containerBase = QFileInfo(file.currentPath).completeBaseName();
        const QString patchedContainerBase = usePatchedBaseTitle(containerBase);
        if (!patchedContainerBase.isEmpty()) {
            return MatchingEngine::extractGameTitle(patchedContainerBase);
        }
        if (!containerBase.isEmpty()) {
            return containerBase;
        }

        const QString entryBase
            = QFileInfo(file.archiveInternalPath.isEmpty() ? file.filename : file.archiveInternalPath)
                  .completeBaseName();
        const QString patchedEntryBase = usePatchedBaseTitle(entryBase);
        if (!patchedEntryBase.isEmpty()) {
            return MatchingEngine::extractGameTitle(patchedEntryBase);
        }
        if (!entryBase.isEmpty()) {
            return entryBase;
        }
    }

    const QString baseName = QFileInfo(file.filename).completeBaseName();
    const QString patchedBase = usePatchedBaseTitle(baseName);
    if (!patchedBase.isEmpty()) {
        return MatchingEngine::extractGameTitle(patchedBase);
    }
    return baseName.isEmpty() ? file.filename : baseName;
}

QStringList orderedMatchHashValues(const QString &preferredHashType, const QString &crc32, const QString &md5,
    const QString &sha1, const QString &sha256) {
    const auto valueForType = [&](const QString &hashType) -> QString {
        if (hashType == QStringLiteral("sha256"))
            return sha256.trimmed();
        if (hashType == QStringLiteral("sha1"))
            return sha1.trimmed();
        if (hashType == QStringLiteral("md5"))
            return md5.trimmed();
        return crc32.trimmed();
    };

    QStringList ordered;
    QSet<QString> seen;
    for (const QString &hashType : VerificationHashMatcher::orderedOfficialHashTypes(preferredHashType)) {
        const QString value = valueForType(hashType);
        if (value.isEmpty())
            continue;
        const QString key = value.toLower();
        if (seen.contains(key))
            continue;
        seen.insert(key);
        ordered.append(value);
    }
    return ordered;
}

} // namespace Remus
