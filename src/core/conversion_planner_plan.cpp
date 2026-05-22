#include "conversion_planner.h"

#include "constants/files.h"
#include "constants/systems.h"

namespace Remus {

namespace {

using Remus::Constants::Files::CHD;
using Remus::Constants::Files::CSO;
using Remus::Constants::Files::GCM;
using Remus::Constants::Files::GDI;
using Remus::Constants::Files::IMG;
using Remus::Constants::Files::ISO;
using Remus::Constants::Files::M3U;
using Remus::Constants::Files::RVZ;
using Remus::Constants::Files::CDI;
using Remus::Constants::Files::CCD;
using Remus::Constants::Files::DAT;
using Remus::Constants::Files::DOL;
using Remus::Constants::Files::ECM;
using Remus::Constants::Files::ELF;
using Remus::Constants::Files::GCZ;
using Remus::Constants::Files::GZ;
using Remus::Constants::Files::ISZ;
using Remus::Constants::Files::LST;
using Remus::Constants::Files::MDS;
using Remus::Constants::Files::PBP;
using Remus::Constants::Files::SUB;
using Remus::Constants::Files::WBFS;
using Remus::Constants::Files::WAD;
namespace Systems = Remus::Constants::Systems;
namespace Files = Remus::Constants::Files;

QString noFallbackRequired() { return QStringLiteral("No fallback required."); }
QString keepOriginalUntilToolAvailable(const QString &toolName)
{
    return QStringLiteral("Keep the original payload untouched until %1 is available.").arg(toolName);
}
QString chainFallback(const QStringList &toolNames)
{
    return QStringLiteral("Keep the original payload untouched until the required toolchain is available: %1.")
        .arg(toolNames.join(QStringLiteral(", ")));
}

bool isChdCanonicalSystem(int systemId)
{
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

bool isRvzCanonicalSystem(int systemId)
{
    return systemId == Systems::ID_GAMECUBE || systemId == Systems::ID_WII;
}

bool isArchiveOnlySystem(int systemId)
{
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

bool isDeferredSystem(int systemId)
{
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

QString systemDisplayName(int systemId)
{
    const Systems::SystemDef *system = Systems::getSystem(systemId);
    return system ? system->displayName : QStringLiteral("Unknown system");
}

ConversionPlanner::Plan makePlan(ConversionPlanner::FormatRole role,
                                 ConversionPlanner::PlannedAction action,
                                 int systemId,
                                 const QString &intermediateExtension,
                                 const QStringList &requiredTools,
                                 const QString &fallbackBehavior,
                                 const QString &reason)
{
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

bool isRecognizedForSystem(int systemId, const QString &extension)
{
    return Systems::getSystemsForExtension(extension).contains(systemId);
}

bool isCanonicalPlaylistExtension(int systemId, const QString &extension)
{
    return extension == M3U && isChdCanonicalSystem(systemId);
}

bool isChdConvertibleExtension(const QString &extension)
{
    return Files::isChdSourceExtension(extension)
        || extension == SUB
        || extension == CCD
        || extension == MDS
        || extension == DAT
        || extension == LST;
}

bool isChdNormalizationExtension(int systemId, const QString &extension)
{
    if (systemId == Systems::ID_PSX && extension == ECM) return true;
    if (systemId == Systems::ID_PS2 && (extension == CSO || extension == GZ || extension == ISZ)) return true;
    if (systemId == Systems::ID_DREAMCAST && extension == CDI) return true;
    return false;
}

bool isRvzNormalizationExtension(const QString &extension)
{
    return extension == WBFS || extension == GCZ || extension == CSO;
}

bool isRvzArchiveOnlyExtension(const QString &extension)
{
    return extension == WAD || extension == DOL || extension == ELF;
}

bool isPs2ArchiveOnlyExtension(const QString &extension)
{
    return extension == ELF;
}

bool isPspNormalizationExtension(const QString &extension)
{
    return extension == CHD;
}

} // namespace

bool ConversionPlanner::Plan::isValid() const
{
    return !ConversionPlanner::toString(role).isEmpty()
        && !ConversionPlanner::toString(action).isEmpty()
        && !fallbackBehavior.trimmed().isEmpty()
        && !reason.trimmed().isEmpty();
}

ConversionPlanner::Plan ConversionPlanner::plan(const Request &request)
{
    const QString extension = normalizedExtension(request.extension);
    const QString systemName = systemDisplayName(request.systemId);

    if (request.systemId <= 0 || extension.isEmpty()) {
        return makePlan(FormatRole::Deferred, PlannedAction::Deferred, request.systemId, {}, {},
                        QStringLiteral("Leave the file untouched until the planner receives a valid system and extension."),
                        QStringLiteral("The planner requires both a valid system id and a normalized extension."));
    }

    if (!isRecognizedForSystem(request.systemId, extension)) {
        return makePlan(FormatRole::Deferred, PlannedAction::Deferred, request.systemId, {}, {},
                        QStringLiteral("Leave the file untouched until the system mapping is clarified."),
                        QStringLiteral("%1 is not registered as a known format for %2.").arg(extension, systemName));
    }

    if (isArchiveOnlySystem(request.systemId)) {
        return makePlan(FormatRole::ArchiveOnly, PlannedAction::ArchiveAsIs, request.systemId, {}, {},
                        QStringLiteral("Archive the payload as-is; no conversion backend is required."),
                        QStringLiteral("%1 media does not have a canonical conversion target in Remus, so %2 stays in the archive-only track.").arg(systemName, extension));
    }

    if (isDeferredSystem(request.systemId)) {
        return makePlan(FormatRole::Deferred, PlannedAction::Deferred, request.systemId, {}, {},
                        QStringLiteral("Leave the payload untouched until policy and tooling are explicit."),
                        QStringLiteral("%1 remains in the deferred policy gate, so %2 is tracked but not yet normalized or exported.").arg(systemName, extension));
    }

    if (isChdCanonicalSystem(request.systemId)) {
        if (extension == CHD || isCanonicalPlaylistExtension(request.systemId, extension)) {
            return makePlan(FormatRole::Canonical, PlannedAction::KeepAsIs, request.systemId, {}, {},
                            noFallbackRequired(),
                            QStringLiteral("%1 is already in the canonical CHD workflow for %2.").arg(extension, systemName));
        }

        if (request.systemId == Systems::ID_PSX && extension == PBP) {
            const PlannedAction action = request.intent == PlanningIntent::ExplicitExport ? PlannedAction::ExportPbp : PlannedAction::NoOp;
            return makePlan(FormatRole::ExportOnly, action, request.systemId, {}, {QStringLiteral("PSXPackager")},
                            keepOriginalUntilToolAvailable(QStringLiteral("PSXPackager")),
                            QStringLiteral("PBP is an explicit PlayStation export format. Remus keeps %1 as the canonical archive and does not promote %2 into the main library target.").arg(CHD, extension));
        }

        if (isChdConvertibleExtension(extension)) {
            return makePlan(FormatRole::Canonical, PlannedAction::ConvertToChd, request.systemId, {}, {QStringLiteral("chdman")},
                            keepOriginalUntilToolAvailable(QStringLiteral("chdman")),
                            QStringLiteral("%1 should convert %2 into CHD as the canonical archive format.").arg(systemName, extension));
        }

        if (isChdNormalizationExtension(request.systemId, extension)) {
            QString reason;
            if (request.systemId == Systems::ID_PS2 && extension == CSO) {
                reason = QStringLiteral("PlayStation 2 CSO files are optional export artifacts. Normalize %1 back to ISO before the canonical CHD path.").arg(extension);
            } else if (request.systemId == Systems::ID_PSX && extension == ECM) {
                reason = QStringLiteral("PlayStation ECM payloads are compatibility inputs. Normalize %1 back to source disc assets before the canonical CHD path.").arg(extension);
            } else if (request.systemId == Systems::ID_DREAMCAST && extension == CDI) {
                reason = QStringLiteral("Dreamcast CDI images are accepted for ingest, but they should normalize to an ISO-like intermediate before the canonical CHD path.");
            } else {
                reason = QStringLiteral("%1 is treated as a normalization-first input for %2 before the canonical CHD path.").arg(extension, systemName);
            }

            QStringList tools{QStringLiteral("chdman")};
            if (request.systemId == Systems::ID_PS2 && extension == CSO) {
                tools.prepend(QStringLiteral("maxcso"));
            }
            return makePlan(FormatRole::NormalizationOnly, PlannedAction::NormalizeToIso, request.systemId, ISO, tools, chainFallback(tools), reason);
        }

        if (isPs2ArchiveOnlyExtension(extension)) {
            return makePlan(FormatRole::ArchiveOnly, PlannedAction::ArchiveAsIs, request.systemId, {}, {},
                            QStringLiteral("Archive the payload as-is; %1 is not part of the canonical disc conversion path.").arg(extension),
                            QStringLiteral("%1 executables stay in the archive-only track even though the main disc workflow is CHD.").arg(systemName));
        }
    }

    if (isRvzCanonicalSystem(request.systemId)) {
        if (extension == RVZ) {
            return makePlan(FormatRole::Canonical, PlannedAction::KeepAsIs, request.systemId, {}, {},
                            noFallbackRequired(),
                            QStringLiteral("%1 is already in the canonical RVZ workflow for %2.").arg(extension, systemName));
        }
        if (extension == ISO || extension == GCM) {
            return makePlan(FormatRole::Canonical, PlannedAction::ConvertToRvz, request.systemId, {}, {QStringLiteral("dolphin-tool")},
                            keepOriginalUntilToolAvailable(QStringLiteral("dolphin-tool")),
                            QStringLiteral("%1 should convert %2 into RVZ as the canonical archive format.").arg(systemName, extension));
        }
        if (isRvzNormalizationExtension(extension)) {
            return makePlan(FormatRole::NormalizationOnly, PlannedAction::NormalizeToIso, request.systemId, ISO,
                            {QStringLiteral("wit"), QStringLiteral("dolphin-tool")},
                            chainFallback({QStringLiteral("wit"), QStringLiteral("dolphin-tool")}),
                            QStringLiteral("%1 is a normalization-first input for %2. Normalize to ISO before the canonical RVZ path.").arg(extension, systemName));
        }
        if (isRvzArchiveOnlyExtension(extension)) {
            return makePlan(FormatRole::ArchiveOnly, PlannedAction::ArchiveAsIs, request.systemId, {}, {},
                            QStringLiteral("Archive the payload as-is; no disc conversion backend is required."),
                            QStringLiteral("%1 is not a canonical disc-image input for %2, so it stays in the archive-only track.").arg(extension, systemName));
        }
    }

    if (request.systemId == Systems::ID_PSP) {
        if (extension == CSO) {
            return makePlan(FormatRole::Canonical, PlannedAction::KeepAsIs, request.systemId, {}, {},
                            noFallbackRequired(),
                            QStringLiteral("%1 is already in the canonical CSO workflow for %2.").arg(extension, systemName));
        }
        if (extension == ISO) {
            return makePlan(FormatRole::Canonical, PlannedAction::ConvertToCso, request.systemId, {}, {QStringLiteral("maxcso")},
                            keepOriginalUntilToolAvailable(QStringLiteral("maxcso")),
                            QStringLiteral("%1 should convert %2 into CSO as the canonical compact output.").arg(systemName, extension));
        }
        if (extension == PBP) {
            return makePlan(FormatRole::ExportOnly, PlannedAction::NoOp, request.systemId, {}, {QStringLiteral("PSXPackager")},
                            keepOriginalUntilToolAvailable(QStringLiteral("PSXPackager")),
                            QStringLiteral("PBP is treated as a compatibility package around PSP workflows, not as the canonical PSP library format."));
        }
        if (isPspNormalizationExtension(extension)) {
            return makePlan(FormatRole::NormalizationOnly, PlannedAction::NormalizeToIso, request.systemId, ISO,
                            {QStringLiteral("chdman"), QStringLiteral("maxcso")},
                            chainFallback({QStringLiteral("chdman"), QStringLiteral("maxcso")}),
                            QStringLiteral("%1 is accepted as an ingest format for PSP, but the canonical compact output remains CSO.").arg(extension));
        }
    }

    return makePlan(FormatRole::Deferred, PlannedAction::Deferred, request.systemId, {}, {},
                    QStringLiteral("Leave the payload untouched until the format policy is made explicit."),
                    QStringLiteral("%1 on %2 is recognized but not yet mapped to a stable planner workflow.").arg(extension, systemName));
}

} // namespace Remus