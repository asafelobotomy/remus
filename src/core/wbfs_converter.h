#pragma once

#include "disc_converter.h"
#include "conversion_result.h"
#include <QString>
#include <QStringList>

namespace Remus {

/**
 * @brief Wraps Wiimms ISO Tools (wit) to convert between WBFS and ISO/GCM.
 *
 * Supported operations:
 *   - ISO/GCM → WBFS  (wit copy)
 *   - WBFS    → ISO   (wit copy)
 *
 * Install:  apt install wiimms-iso-tools  (or brew install wiimms-iso-tools)
 * Tool doc: https://wit.wiimm.de/
 */
class WBFSConverter : public DiscConverter {
    Q_OBJECT

public:
    explicit WBFSConverter(QObject *parent = nullptr);
    ~WBFSConverter() = default;

    bool isWitAvailable() const;
    QString getWitVersion() const;
    void setWitPath(const QString &path);

    /**
     * @brief Convert an ISO or GCM file to WBFS format.
     * @param isoPath   Source .iso or .gcm file
     * @param outputPath Destination .wbfs path; derived from isoPath if empty
     */
    ConversionResult convertIsoToWbfs(const QString &isoPath, const QString &outputPath = QString());

    /**
     * @brief Convert a WBFS file back to a full ISO image.
     * @param wbfsPath   Source .wbfs file
     * @param outputPath Destination .iso path; derived from wbfsPath if empty
     */
    ConversionResult extractWbfsToIso(const QString &wbfsPath, const QString &outputPath = QString());

private:
    QString m_witPath;
};

} // namespace Remus
