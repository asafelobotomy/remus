#include "match_utils.h"

#include <QFileInfo>

#include "constants/constants.h"
#include "patched_rom_parser.h"

namespace Remus {

QString selectBestMatchHash(const FileRecord &file)
{
    if (Constants::Systems::SYSTEMS.contains(file.systemId)) {
        const Constants::Systems::SystemDef &systemDef = Constants::Systems::SYSTEMS[file.systemId];
        const QString preferred = systemDef.preferredHash.toLower();

        if (preferred == "md5" && !file.md5.isEmpty()) return file.md5;
        if (preferred == "sha1" && !file.sha1.isEmpty()) return file.sha1;
        if (preferred == "crc32" && !file.crc32.isEmpty()) return file.crc32;
    }

    if (!file.crc32.isEmpty()) return file.crc32;
    if (!file.sha1.isEmpty()) return file.sha1;
    if (!file.md5.isEmpty()) return file.md5;
    return QString();
}

QString deriveMatchingDisplayName(const FileRecord &file)
{
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
            return patchedContainerBase;
        }
        if (!containerBase.isEmpty()) {
            return containerBase;
        }

        const QString entryBase = QFileInfo(file.archiveInternalPath.isEmpty() ? file.filename
                                                                                : file.archiveInternalPath).completeBaseName();
        const QString patchedEntryBase = usePatchedBaseTitle(entryBase);
        if (!patchedEntryBase.isEmpty()) {
            return patchedEntryBase;
        }
        if (!entryBase.isEmpty()) {
            return entryBase;
        }
    }

    const QString baseName = QFileInfo(file.filename).completeBaseName();
    const QString patchedBase = usePatchedBaseTitle(baseName);
    if (!patchedBase.isEmpty()) {
        return patchedBase;
    }
    return baseName.isEmpty() ? file.filename : baseName;
}

} // namespace Remus