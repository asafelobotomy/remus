#pragma once

#include "disc_converter.h"
#include <QString>
#include <QStringList>

namespace Remus {

enum class CHDCodec {
    LZMA,
    ZLIB,
    FLAC,
    Huffman,
    Auto
};

/**
 * @brief Information about a CHD file
 */
struct CHDInfo {
    QString path;
    int version = 0;
    QString compression;
    qint64 logicalSize = 0;
    qint64 physicalSize = 0;
    QString sha1;
    QString parentSha1;
    int diskType = 0;
};

/**
 * @brief Wrapper for chdman tool to convert disc images to CHD format
 * 
 * CHD (Compressed Hunks of Data) is a lossless compression format that
 * provides 30-60% space savings for disc-based games while maintaining
 * full compatibility with RetroArch and most emulators.
 * 
 * Requires chdman to be installed (part of MAME tools):
 * - Linux: `sudo apt install mame-tools` or `sudo pacman -S mame-tools`
 * - macOS: `brew install mame`
 * - Windows: Download from MAME releases
 */
class CHDConverter : public DiscConverter {
    Q_OBJECT

public:
    explicit CHDConverter(QObject *parent = nullptr);
    ~CHDConverter() = default;

    bool isChdmanAvailable() const;
    QString getChdmanVersion() const;
    void setChdmanPath(const QString &path);
    void setNumProcessors(int numProcessors);
    void setCodec(CHDCodec codec);

    ConversionResult convertCueToCHD(const QString &cuePath,
                                      const QString &outputPath = QString());
    ConversionResult convertIsoToCHD(const QString &isoPath,
                                      const QString &outputPath = QString());
    ConversionResult convertGdiToCHD(const QString &gdiPath,
                                      const QString &outputPath = QString());
    ConversionResult extractCHDToCue(const QString &chdPath,
                                      const QString &outputPath = QString());

    VerifyResult verifyCHD(const QString &chdPath);
    CHDInfo getCHDInfo(const QString &chdPath);

    QList<ConversionResult> batchConvert(const QStringList &inputPaths,
                                          const QString &outputDir = QString());

private:
    ConversionResult runChdman(const QStringList &args,
                                const QString &inputPath,
                                const QString &outputPath);
    QStringList buildCreateCdArgs(const QString &inputPath, const QString &outputPath);
    QString getCodecString() const;

    QString m_chdmanPath;
    int m_numProcessors = 0;
    CHDCodec m_codec = CHDCodec::Auto;
};

} // namespace Remus
