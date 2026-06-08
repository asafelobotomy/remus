#pragma once

#include <QString>
#include <QMap>

#include "exports.h"

namespace Remus {
namespace Constants {
    namespace Cli {

        inline constexpr const char *APPLICATION_NAME = "remus-cli";

        namespace Options {
            inline const QString JSON = QStringLiteral("json");
            inline const QString MOD_JSON = QStringLiteral("mod-json");
            inline const QString MIN_CONFIDENCE = QStringLiteral("min-confidence");
            inline const QString INTERACTIVE = QStringLiteral("interactive");
            inline const QString NO_INTERACTIVE = QStringLiteral("no-interactive");
            inline const QString DRY_RUN_ALL = QStringLiteral("dry-run-all");
            inline const QString EXPORT = QStringLiteral("export");
            inline const QString EXPORT_PATH = QStringLiteral("export-path");
            inline const QString EXPORT_SYSTEMS = QStringLiteral("export-systems");
            inline const QString PROVIDER = QStringLiteral("provider");
        }

        namespace Defaults {
            inline constexpr const char *PROVIDER = "auto";
            inline constexpr const char *CHD_CODEC = "auto";
            inline constexpr const char *BUNDLE_FORMAT = "zip";
            inline constexpr const char *BUNDLE_DISC_FORMAT = "chd";
            inline constexpr const char *PATCH_FORMAT = "bps";
            inline const QString EXPORT_FORMAT = Exports::Formats::CSV;
        }

        /// Pre-configured processing presets for popular emulation frontends.
        /// Each preset selects a bundle format, disc-media format, and folder naming
        /// scheme tuned for the target frontend.
        struct ProcessPreset {
            const char *name;
            const char *displayName;
            const char *bundleFormat; ///< always "zip" (7z is not supported)
            const char *discFormat; ///< "chd" or "original"
            const char *folderNaming; ///< maps to FolderNaming::Scheme
        };

        inline const QMap<QString, ProcessPreset> PROCESS_PRESETS = {
            { QStringLiteral("es-de"), { "es-de", "ES-DE", "zip", "chd", "default" } },
            { QStringLiteral("retrodeck"), { "retrodeck", "RetroDeck", "zip", "chd", "default" } },
            { QStringLiteral("emudeck"), { "emudeck", "EmuDeck (Steam Deck)", "zip", "chd", "emudeck" } },
            { QStringLiteral("batocera"), { "batocera", "Batocera Linux", "zip", "chd", "batocera" } },
            { QStringLiteral("retropie"), { "retropie", "RetroPie", "zip", "chd", "retropie" } },
            { QStringLiteral("romm"), { "romm", "RomM Server", "zip", "original", "romm" } },
        };

        inline const QStringList PROCESS_PRESET_NAMES = {
            QStringLiteral("es-de"),
            QStringLiteral("retrodeck"),
            QStringLiteral("emudeck"),
            QStringLiteral("batocera"),
            QStringLiteral("retropie"),
            QStringLiteral("romm"),
        };

    } // Cli
} // Constants
} // Remus