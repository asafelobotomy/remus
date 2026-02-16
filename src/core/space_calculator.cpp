#include "space_calculator.h"
#include "constants/constants.h"
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QDebug>

namespace Remus {

SpaceCalculator::SpaceCalculator(QObject *parent)
    : QObject(parent)
{
    using namespace Constants::Systems;
    
    // Initialize typical compression ratios (compressed/original)
    // Lower = better compression
    // Using system constants for consistency
    const auto *psx = getSystemByName("PlayStation");
    const auto *ps2 = getSystemByName("PlayStation 2");
    const auto *psp = getSystemByName("PlayStation Portable");
    const auto *dc = getSystemByName("Dreamcast");
    const auto *saturn = getSystemByName("Saturn");
    const auto *segacd = getSystemByName("Sega CD");
    const auto *pcecd = getSystemByName("TurboGrafx-CD");
    const auto *gc = getSystemByName("GameCube");
    const auto *wii = getSystemByName("Wii");
    
    if (psx) m_typicalRatios[psx->internalName] = 0.50;      // ~50% of original
    if (ps2) m_typicalRatios[ps2->internalName] = 0.55;      // ~55% of original
    if (psp) m_typicalRatios[psp->internalName] = 0.60;      // ~60% of original
    if (dc) m_typicalRatios[dc->internalName] = 0.50;        // ~50% of original
    if (saturn) m_typicalRatios[saturn->internalName] = 0.45; // ~45% of original
    if (segacd) m_typicalRatios[segacd->internalName] = 0.45; // ~45% of original
    if (pcecd) m_typicalRatios[pcecd->internalName] = 0.45;   // ~45% of original
    if (gc) m_typicalRatios[gc->internalName] = 0.65;        // ~65% of original (less audio)
    if (wii) m_typicalRatios[wii->internalName] = 0.70;      // ~70% of original
    
    // Systems not yet in constants registry - these should be added to constants later
    m_typicalRatios["3DO"] = 0.50;              // ~50% of original
    m_typicalRatios["Neo Geo CD"] = 0.40;       // ~40% of original
    m_typicalRatios["Xbox"] = 0.65;             // ~65% of original
    m_typicalRatios["Default"] = 0.50;          // Default assumption
}

ConversionStats SpaceCalculator::estimateConversion(const QString &path)
{
    ConversionStats stats;
    stats.path = path;
    stats.converted = false;
    
    QFileInfo info(path);
    if (!info.exists()) {
        return stats;
    }
    
    QString ext = info.suffix().toLower();
    
    // Get total size (including BIN files for CUE)
    if (ext == "cue") {
        stats.format = "BIN/CUE";
        stats.originalSize = info.size();
        
        // Find matching BIN files
        QDir dir = info.absoluteDir();
        QString baseName = info.completeBaseName();
        
        // Look for matching .bin files (could be single or multi-track)
        QStringList binFilters;
        binFilters << "*.bin";
        
        for (const QFileInfo &binInfo : dir.entryInfoList(binFilters, QDir::Files)) {
            // Check if BIN matches CUE base name
            if (binInfo.completeBaseName().startsWith(baseName)) {
                stats.originalSize += binInfo.size();
            }
        }
    } else if (ext == "iso") {
        stats.format = "ISO";
        stats.originalSize = info.size();
    } else if (ext == "gdi") {
        stats.format = "GDI";
        stats.originalSize = info.size();
        
        // GDI files reference multiple track files
        QDir dir = info.absoluteDir();
        QFile gdiFile(path);
        if (gdiFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&gdiFile);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                // GDI lines typically contain: track# offset mode size filename
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 5) {
                    QString trackFile = parts.last();
                    QFileInfo trackInfo(dir.filePath(trackFile));
                    if (trackInfo.exists()) {
                        stats.originalSize += trackInfo.size();
                    }
                }
            }
        }
    } else if (ext == "chd") {
        stats.format = "CHD";
        stats.originalSize = info.size();
        stats.convertedSize = info.size();
        stats.compressionRatio = 1.0;  // Already compressed
        return stats;
    } else {
        stats.format = ext.toUpper();
        stats.originalSize = info.size();
    }
    
    // Estimate converted size based on typical ratios
    QString system = detectSystem(path);
    double ratio = m_typicalRatios.value(system, m_typicalRatios["Default"]);
    
    stats.compressionRatio = ratio;
    stats.convertedSize = static_cast<qint64>(stats.originalSize * ratio);
    stats.savedBytes = stats.originalSize - stats.convertedSize;
    
    return stats;
}

ConversionStats SpaceCalculator::getActualStats(const QString &originalPath,
                                                  const QString &convertedPath)
{
    ConversionStats stats;
    stats.path = originalPath;
    stats.converted = true;
    
    // Get original size (estimate method handles BIN/CUE pairs)
    ConversionStats original = estimateConversion(originalPath);
    stats.originalSize = original.originalSize;
    stats.format = original.format;
    
    // Get actual converted size
    stats.convertedSize = getFileSize(convertedPath);
    stats.savedBytes = stats.originalSize - stats.convertedSize;
    
    if (stats.originalSize > 0) {
        stats.compressionRatio = static_cast<double>(stats.convertedSize) / 
                                  static_cast<double>(stats.originalSize);
    }
    
    return stats;
}

ConversionSummary SpaceCalculator::scanDirectory(const QString &dirPath, bool recursive)
{
    ConversionSummary summary;
    
    QStringList filters;
    filters << "*.cue" << "*.iso" << "*.gdi" << "*.bin" << "*.chd";
    
    QDirIterator::IteratorFlags flags = recursive ? 
        QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    
    QDirIterator it(dirPath, filters, QDir::Files, flags);
    
    // Track processed CUE files to avoid counting their BIN files separately
    QSet<QString> processedBases;
    
    int scanned = 0;
    
    while (it.hasNext()) {
        QString path = it.next();
        QFileInfo info(path);
        QString ext = info.suffix().toLower();
        
        emit scanProgress(++scanned, path);
        
        // Skip standalone BIN files if their CUE was already processed
        if (ext == "bin") {
            QString basePath = info.absolutePath() + "/" + info.completeBaseName();
            if (processedBases.contains(basePath)) {
                continue;
            }
            // Check if a CUE exists for this BIN
            QString cuePath = basePath + ".cue";
            if (QFile::exists(cuePath)) {
                continue;  // Will be counted with CUE
            }
        }
        
        ConversionStats stats = estimateConversion(path);
        
        summary.totalFiles++;
        summary.totalOriginalSize += stats.originalSize;
        
        // Track by format
        summary.sizeByFormat[stats.format] += stats.originalSize;
        summary.countByFormat[stats.format]++;
        
        if (isCHD(path)) {
            summary.convertedFiles++;
            summary.totalConvertedSize += stats.convertedSize;
        } else if (isConvertible(path)) {
            summary.convertibleFiles++;
            summary.totalConvertedSize += stats.convertedSize;  // Estimated
            summary.totalSavedBytes += stats.savedBytes;
        }
        
        // Mark as processed
        if (ext == "cue" || ext == "gdi") {
            processedBases.insert(info.absolutePath() + "/" + info.completeBaseName());
        }
    }
    
    // Calculate average compression ratio
    if (summary.totalOriginalSize > 0) {
        summary.averageCompressionRatio = static_cast<double>(summary.totalConvertedSize) / 
                                           static_cast<double>(summary.totalOriginalSize);
    }
    
    emit scanComplete(summary);
    return summary;
}

bool SpaceCalculator::isConvertible(const QString &path)
{
    QString ext = QFileInfo(path).suffix().toLower();
    return ext == "cue" || ext == "iso" || ext == "gdi" || ext == "img";
}

bool SpaceCalculator::isCHD(const QString &path)
{
    return QFileInfo(path).suffix().toLower() == "chd";
}

double SpaceCalculator::getTypicalRatio(const QString &system)
{
    SpaceCalculator calc;
    return calc.m_typicalRatios.value(system, calc.m_typicalRatios["Default"]);
}

QString SpaceCalculator::formatBytes(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    const qint64 TB = GB * 1024;
    
    if (bytes >= TB) {
        return QString::number(bytes / static_cast<double>(TB), 'f', 2) + " TB";
    } else if (bytes >= GB) {
        return QString::number(bytes / static_cast<double>(GB), 'f', 2) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / static_cast<double>(MB), 'f', 2) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / static_cast<double>(KB), 'f', 2) + " KB";
    } else {
        return QString::number(bytes) + " bytes";
    }
}

QString SpaceCalculator::formatSavingsReport(const ConversionSummary &summary)
{
    QString report;
    
    report += "╔══════════════════════════════════════════╗\n";
    report += "║       CHD Conversion Savings Report      ║\n";
    report += "╚══════════════════════════════════════════╝\n\n";
    
    report += QString("Total files scanned:     %1\n").arg(summary.totalFiles);
    report += QString("Convertible files:       %1\n").arg(summary.convertibleFiles);
    report += QString("Already CHD:             %1\n\n").arg(summary.convertedFiles);
    
    report += QString("Current disk usage:      %1\n").arg(formatBytes(summary.totalOriginalSize));
    report += QString("After conversion:        %1\n").arg(formatBytes(summary.totalConvertedSize));
    report += QString("Estimated savings:       %1\n").arg(formatBytes(summary.totalSavedBytes));
    report += QString("Compression ratio:       %1%\n\n").arg(
        QString::number(summary.averageCompressionRatio * 100, 'f', 1));
    
    if (!summary.countByFormat.isEmpty()) {
        report += "Breakdown by format:\n";
        for (auto it = summary.countByFormat.constBegin(); 
             it != summary.countByFormat.constEnd(); ++it) {
            qint64 size = summary.sizeByFormat.value(it.key(), 0);
            report += QString("  %1: %2 files (%3)\n")
                .arg(it.key(), -10)
                .arg(it.value())
                .arg(formatBytes(size));
        }
    }
    
    return report;
}

qint64 SpaceCalculator::getFileSize(const QString &path) const
{
    QFileInfo info(path);
    return info.exists() ? info.size() : 0;
}

qint64 SpaceCalculator::getDirectorySize(const QString &path, bool recursive) const
{
    qint64 totalSize = 0;
    
    QDirIterator::IteratorFlags flags = recursive ? 
        QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    
    QDirIterator it(path, QDir::Files, flags);
    while (it.hasNext()) {
        it.next();
        totalSize += it.fileInfo().size();
    }
    
    return totalSize;
}

QString SpaceCalculator::detectSystem(const QString &path) const
{
    using namespace Constants::Systems;
    
    QString pathLower = path.toLower();
    
    // Get system names from constants
    const auto *ps2 = getSystemByName("PlayStation 2");
    const auto *psx = getSystemByName("PlayStation");
    const auto *psp = getSystemByName("PlayStation Portable");
    const auto *dc = getSystemByName("Dreamcast");
    const auto *saturn = getSystemByName("Saturn");
    const auto *segacd = getSystemByName("Sega CD");
    const auto *pcecd = getSystemByName("TurboGrafx-CD");
    const auto *gc = getSystemByName("GameCube");
    const auto *wii = getSystemByName("Wii");
    
    // Simple heuristics based on path
    if (ps2 && (pathLower.contains("playstation 2") || pathLower.contains("ps2")))
        return ps2->internalName;
    if (psx && (pathLower.contains("playstation") || pathLower.contains("psx") || pathLower.contains("ps1")))
        return psx->internalName;
    if (psp && pathLower.contains("psp"))
        return psp->internalName;
    if (dc && (pathLower.contains("dreamcast") || pathLower.contains("dc")))
        return dc->internalName;
    if (saturn && pathLower.contains("saturn"))
        return saturn->internalName;
    if (segacd && (pathLower.contains("sega cd") || pathLower.contains("mega cd") || pathLower.contains("segacd")))
        return segacd->internalName;
    if (pcecd && (pathLower.contains("pc engine") || pathLower.contains("turbografx")))
        return pcecd->internalName;
    if (gc && (pathLower.contains("gamecube") || pathLower.contains("gc")))
        return gc->internalName;
    if (wii && pathLower.contains("wii"))
        return wii->internalName;
    
    // Systems not yet in constants registry
    if (pathLower.contains("3do"))
        return "3DO";
    if (pathLower.contains("neo geo cd") || pathLower.contains("neogeocd"))
        return "Neo Geo CD";
    if (pathLower.contains("xbox"))
        return "Xbox";
    
    return "Default";
}

} // namespace Remus
