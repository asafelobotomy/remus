#pragma once

#include "disc_converter.h"
#include "conversion_result.h"
#include <QString>
#include <QStringList>

namespace Remus {

/// Result of a CSO integrity verification.
struct CSOVerifyResult {
    bool valid = false; ///< true if the CSO decompresses cleanly
    qint64 isoSize = 0; ///< size of the decompressed ISO in bytes
    QString error; ///< non-empty on failure
};

class CSOConverter : public DiscConverter {
    Q_OBJECT

public:
    explicit CSOConverter(QObject *parent = nullptr);
    ~CSOConverter() = default;

    bool isMaxcsoAvailable() const;
    QString getMaxcsoVersion() const;
    void setMaxcsoPath(const QString &path);

    ConversionResult convertIsoToCSO(const QString &isoPath, const QString &outputPath = QString());

    ConversionResult extractCSOToIso(const QString &csoPath, const QString &outputPath = QString());

    /**
     * @brief Verify a CSO by decompressing it to a temp dir and checking the
     *        resulting ISO is non-empty and extraction succeeded.
     *
     * maxcso has no standalone verify command, so a round-trip decompress is
     * the most reliable integrity check available.
     */
    CSOVerifyResult verifyCSO(const QString &csoPath);

    QList<ConversionResult> batchConvert(const QStringList &inputPaths, const QString &outputDir = QString());

private:
    QString m_maxcsoPath;
};

} // namespace Remus
