#ifndef REMUS_PATCH_ENGINE_H
#define REMUS_PATCH_ENGINE_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QProcess>

namespace Remus {

/**
 * @brief Supported patch formats
 */
enum class PatchFormat {
    Unknown,
    IPS,        // International Patching System (16 MB limit)
    BPS,        // Beat Patch System (checksums, modern)
    UPS,        // Universal Patching System (alternative to BPS)
    XDelta3,    // XDelta version 3 (large files)
    PPF         // PlayStation Patch Format
};

/**
 * @brief Patch file information
 */
struct PatchInfo {
    QString path;
    PatchFormat format = PatchFormat::Unknown;
    QString formatName;           // Human-readable format name
    qint64 size = 0;
    
    // BPS/UPS checksums (available after parsing header)
    QString sourceChecksum;       // Expected source CRC/checksum
    QString targetChecksum;       // Expected output CRC/checksum
    QString patchChecksum;        // Patch file checksum
    
    bool valid = false;
    QString error;
};

/**
 * @brief Result of a patch operation
 */
struct PatchResult {
    bool success = false;
    QString outputPath;
    QString error;
    
    // Verification info
    bool checksumVerified = false;
    QString calculatedChecksum;
    QString expectedChecksum;
};

/**
 * @brief Applies patches to ROM files
 * 
 * Supports IPS, BPS, UPS, and XDelta3 formats.
 * Uses bundled Flips for IPS/BPS, xdelta3 for XDelta.
 * 
 * Usage:
 *   PatchEngine engine;
 *   PatchInfo info = engine.detectFormat("/path/to/patch.bps");
 *   PatchResult result = engine.apply("/path/to/base.rom", info, "/path/to/output.rom");
 */
class PatchEngine : public QObject {
    Q_OBJECT

public:
    explicit PatchEngine(QObject *parent = nullptr);

    /**
     * @brief Detect patch format from file
     * @param patchPath Path to patch file
     * @return Patch information including format
     */
    PatchInfo detectFormat(const QString &patchPath);

    /**
     * @brief Apply a patch to a ROM file
     * @param basePath Source ROM file
     * @param patch Patch info (from detectFormat)
     * @param outputPath Output path for patched ROM (optional, auto-generates if empty)
     * @return Patch result
     */
    PatchResult apply(const QString &basePath, const PatchInfo &patch,
                      const QString &outputPath = QString());

    /**
     * @brief Create a patch between two files
     * @param originalPath Original (unmodified) ROM
     * @param modifiedPath Modified ROM
     * @param patchPath Output patch file path
     * @param format Desired patch format
     * @return True if successful
     */
    bool createPatch(const QString &originalPath, const QString &modifiedPath,
                     const QString &patchPath, PatchFormat format = PatchFormat::BPS);

    /**
     * @brief Check if patching tools are available
     * @return Map of format name to availability
     */
    QMap<QString, bool> checkToolAvailability();

    /**
     * @brief Get path to Flips executable
     * @return Path to flips binary, or empty if not found
     */
    QString getFlipsPath();

    /**
     * @brief Get path to xdelta3 executable
     * @return Path to xdelta3 binary, or empty if not found
     */
    QString getXdelta3Path();

    /**
     * @brief Get path to PPF patcher executable
     * @return Path to applyppf/ppf3 binary, or empty if not found
     */
    QString getPpfPath();

    /**
     * @brief Set custom path for Flips
     * @param path Path to flips binary
     */
    void setFlipsPath(const QString &path);

    /**
     * @brief Set custom path for xdelta3
     * @param path Path to xdelta3 binary
     */
    void setXdelta3Path(const QString &path);

    /**
     * @brief Set custom path for PPF patcher
     * @param path Path to applyppf/ppf3 binary
     */
    void setPpfPath(const QString &path);

    /**
     * @brief Check if format is supported
     * @param format Patch format to check
     * @return True if format can be applied
     */
    bool isFormatSupported(PatchFormat format);

    /**
     * @brief Get format from file extension
     * @param extension File extension (with or without dot)
     * @return Detected format
     */
    static PatchFormat formatFromExtension(const QString &extension);

    /**
     * @brief Get format name string
     * @param format Patch format
     * @return Human-readable format name
     */
    static QString formatName(PatchFormat format);

signals:
    void patchProgress(int percentage);
    void patchError(const QString &error);
    void patchComplete(const PatchResult &result);

private:
    QString m_flipsPath;
    QString m_xdelta3Path;
    QString m_ppfPath;

    PatchResult applyIPS(const QString &basePath, const QString &patchPath, 
                          const QString &outputPath);
    PatchResult applyBPS(const QString &basePath, const QString &patchPath,
                          const QString &outputPath);
    PatchResult applyXDelta(const QString &basePath, const QString &patchPath,
                             const QString &outputPath);
    PatchResult applyPPF(const QString &basePath, const QString &patchPath,
                          const QString &outputPath);
    
    // Built-in IPS implementation (fallback if no Flips)
    PatchResult applyIPSBuiltin(const QString &basePath, const QString &patchPath,
                                 const QString &outputPath);
    
    QString findExecutable(const QString &name);
    QString generateOutputPath(const QString &basePath, const QString &patchPath);
};

} // namespace Remus

#endif // REMUS_PATCH_ENGINE_H
