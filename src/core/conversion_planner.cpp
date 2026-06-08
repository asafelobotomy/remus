#include "conversion_planner.h"

#include "constants/files.h"
#include "constants/systems.h"

namespace Remus {

namespace {

    using Remus::Constants::Files::CCD;
    using Remus::Constants::Files::CDI;
    using Remus::Constants::Files::CHD;
    using Remus::Constants::Files::CSO;
    using Remus::Constants::Files::DAT;
    using Remus::Constants::Files::DOL;
    using Remus::Constants::Files::ECM;
    using Remus::Constants::Files::ELF;
    using Remus::Constants::Files::GCM;
    using Remus::Constants::Files::GCZ;
    using Remus::Constants::Files::GDI;
    using Remus::Constants::Files::GZ;
    using Remus::Constants::Files::IMG;
    using Remus::Constants::Files::ISO;
    using Remus::Constants::Files::ISZ;
    using Remus::Constants::Files::LST;
    using Remus::Constants::Files::M3U;
    using Remus::Constants::Files::MDS;
    using Remus::Constants::Files::PBP;
    using Remus::Constants::Files::RVZ;
    using Remus::Constants::Files::SUB;
    using Remus::Constants::Files::WAD;
    using Remus::Constants::Files::WBFS;
    namespace Files = Remus::Constants::Files;
    namespace Systems = Remus::Constants::Systems;

    QString noFallbackRequired() {
        return QStringLiteral("No fallback required.");
    }

    QString keepOriginalUntilToolAvailable(const QString &toolName) {
        return QStringLiteral("Keep the original payload untouched until %1 is available.").arg(toolName);
    }

    QString chainFallback(const QStringList &toolNames) {
        return QStringLiteral("Keep the original payload untouched until the required toolchain is available: %1.")
            .arg(toolNames.join(QStringLiteral(", ")));
    }

    bool isChdCanonicalSystem(int systemId) {
        switch (systemId) {
        case Systems::ID_PSX:
        case Systems::ID_PS2:
        case Systems::ID_SATURN:
        case Systems::ID_SEGA_CD:
        case Systems::ID_TURBOGRAFX_CD:
        case Systems::ID_DREAMCAST:
        case Systems::ID_3DO:
        case Systems::ID_NEO_GEO_CD:
        case Systems::ID_PC_FX:
        case Systems::ID_CDI:
        case Systems::ID_CD32:
        case Systems::ID_NAOMI:
        case Systems::ID_ATARI_JAGUAR_CD:
            return true;
        default:
            return false;
        }
    }

    bool isRvzCanonicalSystem(int systemId) {
        return systemId == Systems::ID_GAMECUBE || systemId == Systems::ID_WII;
    }

    bool isArchiveOnlySystem(int systemId) {
        switch (systemId) {
        case Systems::ID_NES:
        case Systems::ID_SNES:
        case Systems::ID_N64:
        case Systems::ID_GB:
        case Systems::ID_GBC:
        case Systems::ID_GBA:
        case Systems::ID_NDS:
        case Systems::ID_GENESIS:
        case Systems::ID_MASTER_SYSTEM:
        case Systems::ID_ATARI_2600:
        case Systems::ID_ATARI_7800:
        case Systems::ID_LYNX:
        case Systems::ID_TURBOGRAFX16:
        case Systems::ID_NEO_GEO:
        case Systems::ID_GAME_GEAR:
        case Systems::ID_32X:
        case Systems::ID_ATARI_JAGUAR:
        case Systems::ID_NGP:
        case Systems::ID_WONDERSWAN:
        case Systems::ID_VIRTUAL_BOY:
        case Systems::ID_C64:
        case Systems::ID_AMIGA:
        case Systems::ID_ZX_SPECTRUM:
        case Systems::ID_SUPERGRAFX:
        case Systems::ID_FDS:
        case Systems::ID_ATARI_5200:
        case Systems::ID_ATARI_8BIT:
        case Systems::ID_ATARI_ST:
        case Systems::ID_COLECOVISION:
        case Systems::ID_INTELLIVISION:
        case Systems::ID_MSX:
        case Systems::ID_MSX2:
        case Systems::ID_SG1000:
        // New systems (IDs 58-82) — all ROM/disk-image based, no CHD conversion required
        case Systems::ID_AMSTRAD_CPC:
        case Systems::ID_ENTERPRISE_128:
        case Systems::ID_ZX81:
        case Systems::ID_VIDEOTON_TVC:
        case Systems::ID_SEGA_PICO:
        case Systems::ID_VIC20:
        case Systems::ID_ODYSSEY2:
        case Systems::ID_SUPERVISION:
        case Systems::ID_POCKET_CHALLENGE_V2:
        case Systems::ID_PC98:
        case Systems::ID_INTERTON_VC4000:
        case Systems::ID_ARCADIA_2001:
        case Systems::ID_VECTREX:
        case Systems::ID_POKEMON_MINI:
        case Systems::ID_CHANNEL_F:
        case Systems::ID_SCV:
        case Systems::ID_GP32:
        case Systems::ID_GAMECOM:
        case Systems::ID_STUDIO_II:
        case Systems::ID_ATOMISWAVE:
        case Systems::ID_CASIO_PV1000:
        case Systems::ID_SUPER_ACAN:
        case Systems::ID_CASIO_LOOPY:
        case Systems::ID_SHARP_X1:
        case Systems::ID_X68000:
            return true;
        default:
            return false;
        }
    }

    bool isDeferredSystem(int systemId) {
        switch (systemId) {
        case Systems::ID_3DS:
        case Systems::ID_SWITCH:
        case Systems::ID_PSVITA:
        case Systems::ID_XBOX:
        case Systems::ID_XBOX360:
        case Systems::ID_ARCADE:
        case Systems::ID_WIIU:
        case Systems::ID_PS3:
            return true;
        default:
            return false;
        }
    }

    QString systemDisplayName(int systemId) {
        const Systems::SystemDef *system = Systems::getSystem(systemId);
        return system ? system->displayName : QStringLiteral("Unknown system");
    }

    ConversionPlanner::Plan makePlan(ConversionPlanner::FormatRole role, ConversionPlanner::PlannedAction action,
        int systemId, const QString &intermediateExtension, const QStringList &requiredTools,
        const QString &fallbackBehavior, const QString &reason) {
        ConversionPlanner::Plan plan;
        plan.role = role;
        plan.action = action;
        if (role != ConversionPlanner::FormatRole::ArchiveOnly && role != ConversionPlanner::FormatRole::Deferred) {
            plan.canonicalExtension = ConversionPlanner::canonicalExtensionForSystem(systemId);
        }
        plan.intermediateExtension = intermediateExtension;
        plan.requiredTools = requiredTools;
        plan.fallbackBehavior = fallbackBehavior;
        plan.reason = reason;
        return plan;
    }

    bool isRecognizedForSystem(int systemId, const QString &extension) {
        return Systems::getSystemsForExtension(extension).contains(systemId);
    }

    bool isCanonicalPlaylistExtension(int systemId, const QString &extension) {
        return extension == M3U && isChdCanonicalSystem(systemId);
    }

    bool isChdConvertibleExtension(const QString &extension) {
        return Files::isChdSourceExtension(extension) || extension == SUB || extension == CCD || extension == MDS
            || extension == DAT || extension == LST;
    }

    bool isChdNormalizationExtension(int systemId, const QString &extension) {
        if (systemId == Systems::ID_PSX && extension == ECM) {
            return true;
        }

        if (systemId == Systems::ID_PS2 && (extension == CSO || extension == GZ || extension == ISZ)) {
            return true;
        }

        if (systemId == Systems::ID_DREAMCAST && extension == CDI) {
            return true;
        }

        return false;
    }

    bool isRvzNormalizationExtension(const QString &extension) {
        return extension == WBFS || extension == GCZ || extension == CSO;
    }

    bool isRvzArchiveOnlyExtension(const QString &extension) {
        return extension == WAD || extension == DOL || extension == ELF;
    }

    bool isPs2ArchiveOnlyExtension(const QString &extension) {
        return extension == ELF;
    }

    bool isPspNormalizationExtension(const QString &extension) {
        return extension == CHD;
    }

} // namespace

QString ConversionPlanner::canonicalExtensionForSystem(int systemId) {
    if (isChdCanonicalSystem(systemId)) {
        return CHD;
    }

    if (isRvzCanonicalSystem(systemId)) {
        return RVZ;
    }

    if (systemId == Systems::ID_PSP) {
        return CSO;
    }

    return { };
}

bool ConversionPlanner::isSystemClassified(int systemId) {
    return Constants::Systems::getSystem(systemId) != nullptr;
}

QString ConversionPlanner::normalizedExtension(const QString &extension) {
    QString normalized = extension.trimmed().toLower();
    if (normalized.isEmpty()) {
        return normalized;
    }

    if (!normalized.startsWith('.')) {
        normalized.prepend('.');
    }

    return normalized;
}

QString ConversionPlanner::toString(FormatRole role) {
    switch (role) {
    case FormatRole::Canonical:
        return QStringLiteral("canonical");
    case FormatRole::NormalizationOnly:
        return QStringLiteral("normalization-only");
    case FormatRole::ExportOnly:
        return QStringLiteral("export-only");
    case FormatRole::ArchiveOnly:
        return QStringLiteral("archive-only");
    case FormatRole::Deferred:
        return QStringLiteral("deferred");
    }

    return { };
}

QString ConversionPlanner::toString(PlannedAction action) {
    switch (action) {
    case PlannedAction::KeepAsIs:
        return QStringLiteral("keep-as-is");
    case PlannedAction::ConvertToChd:
        return QStringLiteral("convert-to-chd");
    case PlannedAction::ConvertToRvz:
        return QStringLiteral("convert-to-rvz");
    case PlannedAction::ConvertToCso:
        return QStringLiteral("convert-to-cso");
    case PlannedAction::NormalizeToIso:
        return QStringLiteral("normalize-to-iso");
    case PlannedAction::ExportPbp:
        return QStringLiteral("export-pbp");
    case PlannedAction::ArchiveAsIs:
        return QStringLiteral("archive-as-is");
    case PlannedAction::NoOp:
        return QStringLiteral("no-op");
    case PlannedAction::Deferred:
        return QStringLiteral("deferred");
    }

    return { };
}

} // namespace Remus