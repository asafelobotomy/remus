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
namespace Systems = Remus::Constants::Systems;
namespace Files = Remus::Constants::Files;

QString pbpExtension()
{
    return QStringLiteral(".pbp");
}

QString wbfsExtension()
{
    return QStringLiteral(".wbfs");
}

QString gczExtension()
{
    return QStringLiteral(".gcz");
}

QString wadExtension()
{
    return QStringLiteral(".wad");
}

QString elfExtension()
{
    return QStringLiteral(".elf");
}

QString iszExtension()
{
    return QStringLiteral(".isz");
}

QString gzExtension()
{
    return QStringLiteral(".gz");
}

QString ecmExtension()
{
    return QStringLiteral(".ecm");
}

QString cdiExtension()
{
    return QStringLiteral(".cdi");
}

QString datExtension()
{
    return QStringLiteral(".dat");
}

QString lstExtension()
{
    return QStringLiteral(".lst");
}

QString subExtension()
{
    return QStringLiteral(".sub");
}

QString ccdExtension()
{
    return QStringLiteral(".ccd");
}

QString mdsExtension()
{
    return QStringLiteral(".mds");
}

QString dolExtension()
{
    return QStringLiteral(".dol");
}

QString noFallbackRequired()
{
    return QStringLiteral("No fallback required.");
}

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
    if (role != ConversionPlanner::FormatRole::ArchiveOnly
        && role != ConversionPlanner::FormatRole::Deferred) {
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
        || extension == subExtension()
        || extension == ccdExtension()
        || extension == mdsExtension()
        || extension == datExtension()
        || extension == lstExtension();
}

bool isChdNormalizationExtension(int systemId, const QString &extension)
{
    if (systemId == Systems::ID_PSX && extension == ecmExtension()) {
        return true;
    }

    if (systemId == Systems::ID_PS2
        && (extension == CSO || extension == gzExtension() || extension == iszExtension())) {
        return true;
    }

    if (systemId == Systems::ID_DREAMCAST && extension == cdiExtension()) {
        return true;
    }

    return false;
}

bool isRvzNormalizationExtension(const QString &extension)
{
    return extension == wbfsExtension() || extension == gczExtension() || extension == CSO;
}

bool isRvzArchiveOnlyExtension(const QString &extension)
{
    return extension == wadExtension() || extension == dolExtension() || extension == elfExtension();
}

bool isPs2ArchiveOnlyExtension(const QString &extension)
{
    return extension == elfExtension();
}

bool isPspNormalizationExtension(const QString &extension)
{
    return extension == CHD;
}

} // namespace

QString ConversionPlanner::canonicalExtensionForSystem(int systemId)
{
    if (isChdCanonicalSystem(systemId)) {
        return CHD;
    }

    if (isRvzCanonicalSystem(systemId)) {
        return RVZ;
    }

    if (systemId == Systems::ID_PSP) {
        return CSO;
    }

    return {};
}

bool ConversionPlanner::isSystemClassified(int systemId)
{
    return isChdCanonicalSystem(systemId)
        || isRvzCanonicalSystem(systemId)
        || systemId == Systems::ID_PSP
        || isArchiveOnlySystem(systemId)
        || isDeferredSystem(systemId);
}

QString ConversionPlanner::normalizedExtension(const QString &extension)
{
    QString normalized = extension.trimmed().toLower();
    if (normalized.isEmpty()) {
        return normalized;
    }

    if (!normalized.startsWith('.')) {
        normalized.prepend('.');
    }

    return normalized;
}

QString ConversionPlanner::toString(FormatRole role)
{
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

    return {};
}

QString ConversionPlanner::toString(PlannedAction action)
{
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

    return {};
}

} // namespace Remus