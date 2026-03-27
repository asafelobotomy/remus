#pragma once

#include "disc_converter.h"
#include <QString>
#include <QStringList>

namespace Remus {

enum class RVZCompression {
    Zstd,       // Best balance of speed and size (default)
    Bzip2,      // Smaller but slower decompression
    LZMA,       // Highest compression, slowest
    LZMA2,      // Improved LZMA
    None,       // No compression
    Auto        // Let dolphin-tool decide (zstd level 5)
};

class RVZConverter : public DiscConverter {
    Q_OBJECT

public:
    explicit RVZConverter(QObject *parent = nullptr);
    ~RVZConverter() = default;

    bool isDolphinToolAvailable() const;
    QString getDolphinToolVersion() const;
    void setDolphinToolPath(const QString &path);

    void setCompression(RVZCompression compression);
    void setCompressionLevel(int level);

    ConversionResult convertIsoToRVZ(const QString &isoPath,
                                      const QString &outputPath = QString());

    ConversionResult extractRVZToIso(const QString &rvzPath,
                                      const QString &outputPath = QString());

    VerifyResult verifyRVZ(const QString &rvzPath);

    QList<ConversionResult> batchConvert(const QStringList &inputPaths,
                                          const QString &outputDir = QString());

private:
    QString getCompressionString() const;

    QString m_dolphinToolPath;
    RVZCompression m_compression = RVZCompression::Auto;
    int m_compressionLevel = 5;  // zstd default sweet spot
};

} // namespace Remus
