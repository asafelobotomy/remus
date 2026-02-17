#pragma once

#include <QStringList>

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
    QString lastChdCodec = "auto";

    QString lastArchivePath;
    QString lastArchiveOut;

    QString lastPatchBase;
    QString lastPatchFile;
    QString lastPatchOutput;
    QString lastPatchOriginal;
    QString lastPatchModified;
    QString lastPatchFormat = "bps";

    QString lastExportFormat = "csv";
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
