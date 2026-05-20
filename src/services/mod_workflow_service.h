#ifndef REMUS_MOD_WORKFLOW_SERVICE_H
#define REMUS_MOD_WORKFLOW_SERVICE_H

#include <functional>
#include <QString>
#include <QTemporaryDir>
#include "../core/database.h"
#include "patch_service.h"
#include "mod_catalog_provider.h"

namespace Remus {

struct ModInstallResult {
    bool    success = false;
    QString error;
    QString patchedRomPath;
    QString bundlePath;
    int     patchedFileId = 0;
};

class ModWorkflowService {
public:
    using ProgressCallback = std::function<void(const QString &stage, int percent)>;

    ModWorkflowService(Database &db, PatchService &patchService);

    /// Mark that the active catalog was loaded from a remote URL.
    /// When set, file:// and relative patch sources are rejected.
    void setCatalogIsRemote(bool isRemote) { m_catalogIsRemote = isRemote; }

    /// Install a mod for a matched file.
    ///
    /// Steps:
    /// 1. Resolve patch file from modEntry.patchUrl (file://, local path, or HTTP(S))
    /// 2. Verify patch SHA1 against modEntry.patchSha1
    /// 3. Extract base ROM if compressed
    /// 4. Apply patch via PatchService::apply()
    /// 5. Insert patched ROM as new files row (is_patched=true, parent_file_id)
    /// 6. Record in mod_installations table
    ///
    /// The original FileRecord is NEVER modified.
    ModInstallResult install(const FileRecord           &baseFile,
                             const ModEntry              &mod,
                             const QString               &outputDir,
                             ProgressCallback             cb = nullptr);

    /// List installed mods for a base file.
    QList<Database::ModInstallationRecord> getInstalledMods(int baseFileId);

    /// Uninstall a mod (delete patched file + remove DB records).
    bool uninstall(int modInstallationId);

private:
    QString resolvePatchPath(const QString &patchUrl, QString &error,
                             ProgressCallback cb = nullptr);
    QString downloadPatch(const QUrl &url, QString &error,
                          ProgressCallback cb = nullptr);
    bool    verifySha1(const QString &filePath, const QString &expectedSha1);

    Database     &m_db;
    PatchService &m_patchSvc;
    std::unique_ptr<QTemporaryDir> m_downloadDir;
    bool m_catalogIsRemote = false;
};

} // namespace Remus

#endif // REMUS_MOD_WORKFLOW_SERVICE_H
