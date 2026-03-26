#pragma once

#include <QStringList>

#include "../core/constants/constants.h"

namespace Remus::Cli {

struct SessionState {
    QString lastScanPath;
    bool lastDoHash = true;
    bool lastDoMatch = true;
    bool lastDoOrganize = false;
    QString lastOrganizeDest;
    bool lastDryRun = false;

    QString lastChdInput;
    QString lastChdOutputDir;
    QString lastChdCodec = QString::fromLatin1(Constants::Cli::Defaults::CHD_CODEC);

    QString lastArchivePath;
    QString lastArchiveOut;

    QString lastPatchBase;
    QString lastPatchFile;
    QString lastPatchOutput;
    QString lastPatchOriginal;
    QString lastPatchModified;
    QString lastPatchFormat = QString::fromLatin1(Constants::Cli::Defaults::PATCH_FORMAT);

    QString lastExportFormat = Constants::Cli::Defaults::EXPORT_FORMAT;
    QString lastExportPath;
    QString lastExportSystems;
    bool lastExportDryRun = true;

    QString lastTemplate;
};

struct InteractiveResult {
    bool valid = false;
    QStringList args;
};

class InteractiveSession {
public:
    InteractiveResult run();

    // Test helpers for state round-trip without entering TUI
    static SessionState loadStateSnapshot();
    static void saveStateSnapshot(const SessionState &state);

private:
    SessionState loadState();
    void saveState(const SessionState &state);
};

} // namespace Remus::Cli
