#pragma once

#include "external_tool_runner.h"
#include "conversion_result.h"

namespace Remus {

/**
 * @brief Intermediate base class for disc image format converters.
 *
 * Consolidates shared signals, helper utilities, and the run-tool-and-build-result
 * pattern used by CHDConverter, RVZConverter, and CSOConverter.
 */
class DiscConverter : public ExternalToolRunner {
    Q_OBJECT

public:
    explicit DiscConverter(QObject *parent = nullptr);

    void cancel() override;

signals:
    void conversionStarted(const QString &inputPath, const QString &outputPath);
    void conversionProgress(int percent, const QString &status);
    void conversionCompleted(const ConversionResult &result);
    void batchProgress(int completed, int total);
    void conversionCancelled();
    void errorOccurred(const QString &error);

protected:
    /**
     * @brief Run an external tool and build a ConversionResult from the outcome.
     *
     * Emits conversionStarted, errorOccurred (on failure), and conversionCompleted.
     *
     * @param toolPath        Path or name of the external binary
     * @param args            Arguments to pass
     * @param toolDisplayName Human-readable name for log messages
     * @param inputPath       Source file (used in result and size calc)
     * @param outputPath      Destination file
     * @param inputSize       Pre-calculated input size, or -1 to auto-detect
     * @param timeoutMs       Process timeout (default 30 min)
     */
    ConversionResult runToolConversion(const QString &toolPath, const QStringList &args, const QString &toolDisplayName,
        const QString &inputPath, const QString &outputPath, qint64 inputSize = -1, int timeoutMs = 1800000);

    static qint64 getFileSize(const QString &path);
    static QString getDefaultOutputPath(const QString &inputPath, const QString &targetExt);
};

} // namespace Remus
