#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>

namespace Remus {

/**
 * @brief Conversion statistics for a single file
 */
struct ConversionStats {
    QString path;
    QString format;              // "BIN/CUE", "ISO", "GDI", "CHD"
    qint64 originalSize = 0;     // Size before conversion
    qint64 convertedSize = 0;    // Size after conversion (0 if estimate)
    qint64 savedBytes = 0;       // Bytes saved (originalSize - convertedSize)
    double compressionRatio = 0.0;  // convertedSize / originalSize
    bool converted = false;      // True if actually converted, false if estimate
};

/**
 * @brief Summary of conversion savings
 */
struct ConversionSummary {
    int totalFiles = 0;
    int convertibleFiles = 0;    // Files that can be converted
    int convertedFiles = 0;      // Files already converted
    
    qint64 totalOriginalSize = 0;
    qint64 totalConvertedSize = 0;
    qint64 totalSavedBytes = 0;
    double averageCompressionRatio = 0.0;
    
    // By format breakdown
    QMap<QString, qint64> sizeByFormat;
    QMap<QString, int> countByFormat;
};

/**
 * @brief Utility for calculating and reporting space savings from CHD conversion
 */
class SpaceCalculator : public QObject {
    Q_OBJECT

public:
    explicit SpaceCalculator(QObject *parent = nullptr);
    ~SpaceCalculator() = default;

    /**
     * @brief Estimate compression for a disc image
     * 
     * Uses average compression ratios:
     * - PlayStation/PS2: 40-50% compression
     * - Dreamcast: 40-55% compression
     * - Sega CD/Saturn: 35-45% compression
     * - PC Engine CD: 35-50% compression
     * 
     * @param path Path to disc image
     * @return Estimated statistics
     */
    ConversionStats estimateConversion(const QString &path);

    /**
     * @brief Get actual conversion stats from completed conversion
     */
    ConversionStats getActualStats(const QString &originalPath,
                                    const QString &convertedPath);

    /**
     * @brief Scan directory and estimate total savings
     * @param dirPath Directory to scan
     * @param recursive Scan subdirectories
     * @return Summary of potential savings
     */
    ConversionSummary scanDirectory(const QString &dirPath, bool recursive = true);

    /**
     * @brief Check if file can be converted to CHD
     */
    static bool isConvertible(const QString &path);

    /**
     * @brief Check if file is already CHD
     */
    static bool isCHD(const QString &path);

    /**
     * @brief Get typical compression ratio for a system
     */
    static double getTypicalRatio(const QString &system);

    /**
     * @brief Format bytes as human-readable string
     */
    static QString formatBytes(qint64 bytes);

    /**
     * @brief Format savings as human-readable report
     */
    QString formatSavingsReport(const ConversionSummary &summary);

signals:
    void scanProgress(int filesScanned, const QString &currentFile);
    void scanComplete(const ConversionSummary &summary);

private:
    qint64 getFileSize(const QString &path) const;
    qint64 getDirectorySize(const QString &path, bool recursive) const;
    QString detectSystem(const QString &path) const;
    
    // Typical compression ratios by system
    QMap<QString, double> m_typicalRatios;
};

} // namespace Remus
