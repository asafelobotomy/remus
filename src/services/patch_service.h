#ifndef REMUS_PATCH_SERVICE_H
#define REMUS_PATCH_SERVICE_H

#include <functional>
#include <QString>
#include <QStringList>
#include <QMap>

#include "../core/patch_engine.h"

namespace Remus {

/**
 * @brief Shared patching service (non-QObject, callback-based)
 *
 * Wraps PatchEngine for detecting, applying, and creating patches.
 * Usable by both GUI controllers and TUI screens.
 */
class PatchService {
public:
    using ProgressCallback = std::function<void(int percent)>;
    using LogCallback      = std::function<void(const QString &message)>;

    PatchService();
    ~PatchService();

    // ── Patch Detection ───────────────────────────────────

    /**
     * @brief Detect the format of a patch file
     */
    PatchInfo detectFormat(const QString &patchPath);

    /**
     * @brief Check if a format is supported (tools available)
     */
    bool isFormatSupported(PatchFormat format) const;

    /**
     * @brief Get list of currently supported format names
     */
    QStringList getSupportedFormats() const;

    // ── Apply Patch ───────────────────────────────────────

    /**
     * @brief Apply a single patch
     * @param basePath   ROM file to patch
     * @param patchPath  Patch file path
     * @param outputPath Output path (empty = in-place or auto-named)
     * @param progressCb Progress callback (percent)
     * @return Patch result
     */
    PatchResult apply(const QString &basePath,
                      const QString &patchPath,
                      const QString &outputPath = {},
                      ProgressCallback progressCb = nullptr);

    /**
     * @brief Apply a patch using pre-detected PatchInfo
     */
    PatchResult apply(const QString &basePath,
                      const PatchInfo &info,
                      const QString &outputPath = {},
                      ProgressCallback progressCb = nullptr);

    /**
     * @brief Batch-apply multiple patches to the same base
     * @param basePath    ROM file to patch
     * @param patchPaths  List of patch files
     * @param progressCb  Per-patch progress
     * @param logCb       Optional log callback
     * @return List of results (one per patch)
     */
    QList<PatchResult> batchApply(const QString &basePath,
                                  const QStringList &patchPaths,
                                  ProgressCallback progressCb = nullptr,
                                  LogCallback logCb = nullptr);

    // ── Create Patch ──────────────────────────────────────

    /**
     * @brief Create a patch from two files
     */
    bool createPatch(const QString &originalPath,
                     const QString &modifiedPath,
                     const QString &patchPath,
                     PatchFormat format);

    // ── Tool Management ───────────────────────────────────

    /**
     * @brief Get tool availability map
     */
    QMap<QString, bool> getToolStatus() const;

    /**
     * @brief Set custom flips path
     */
    void setFlipsPath(const QString &path);

    /**
     * @brief Set custom xdelta3 path
     */
    void setXdelta3Path(const QString &path);

    /**
     * @brief Set custom ppf path
     */
    void setPpfPath(const QString &path);

    /**
     * @brief Get current tool paths
     */
    QString getFlipsPath() const;
    QString getXdelta3Path() const;
    QString getPpfPath() const;

    /**
     * @brief Generate an output path based on base + patch names
     */
    static QString generateOutputPath(const QString &basePath,
                                      const QString &patchPath);

    /**
     * @brief Access underlying PatchEngine (for advanced use)
     */
    PatchEngine *engine() { return m_engine; }

private:
    PatchEngine *m_engine = nullptr;
};

} // namespace Remus

#endif // REMUS_PATCH_SERVICE_H
