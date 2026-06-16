#pragma once

#include "disc_converter.h"
#include "conversion_result.h"
#include <QString>
#include <QStringList>

namespace Remus {

/**
 * @brief Wraps PSXPackager to export PS1 disc images as PBP files.
 *
 * PBP is not a canonical archive format — it is an explicit export path for
 * PSP / PS3 compatibility workflows.  The PS1 canonical library format remains
 * CHD.  Conversion is one-way; source files are never modified.
 *
 * Supported inputs:
 *   - .cue  (single-disc BIN/CUE)
 *   - .iso  (single-disc ISO)
 *   - .m3u  (multi-disc playlist — each referenced disc is packaged together)
 *
 * Install: https://github.com/nicholasstephan/psxpackager
 *          (or your distribution's psxpackager / PSXPackager package)
 */
class PBPExporter : public DiscConverter {
    Q_OBJECT

public:
    explicit PBPExporter(QObject *parent = nullptr);
    ~PBPExporter() override = default;

    bool isPSXPackagerAvailable() const;
    QString getPSXPackagerVersion() const;
    void setPSXPackagerPath(const QString &path);

    /**
     * @brief Export a PS1 disc image or multi-disc playlist to PBP.
     * @param sourcePath  .cue, .iso, or .m3u file
     * @param outputPath  Destination .pbp path; derived from sourcePath if empty
     */
    ConversionResult exportToPBP(const QString &sourcePath, const QString &outputPath = QString());

private:
    QString m_psxPackagerPath;
};

} // namespace Remus
