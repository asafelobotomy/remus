#include "conversion_controller.h"
#include <QDebug>
#include <QFileInfo>
#include <QDir>

namespace Remus {

ConversionController::ConversionController(Database *db, QObject *parent)
    : QObject(parent), m_db(db)
{
    m_chdConverter = new CHDConverter(this);
    m_archiveExtractor = new ArchiveExtractor(this);
    
    // Connect CHD converter signals
    connect(m_chdConverter, &CHDConverter::conversionProgress, this, [this](int percent, const QString &) {
        emit conversionProgress(percent);
    });
    
    connect(m_chdConverter, &CHDConverter::errorOccurred, this, [this](const QString &error) {
        emit conversionError(error);
        m_converting = false;
        emit convertingChanged();
    });
}

void ConversionController::convertToCHD(const QString &path, const QString &codec)
{
    if (m_converting) {
        qWarning() << "Conversion already in progress";
        return;
    }
    
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        emit conversionError("File not found: " + path);
        return;
    }
    
    m_converting = true;
    emit convertingChanged();
    
    qDebug() << "Converting to CHD:" << path << "with codec:" << codec;
    
    // Set codec based on string
    CHDCodec chdCodec = CHDCodec::Auto;
    if (codec == "lzma") chdCodec = CHDCodec::LZMA;
    else if (codec == "zlib") chdCodec = CHDCodec::ZLIB;
    else if (codec == "flac") chdCodec = CHDCodec::FLAC;
    else if (codec == "huffman") chdCodec = CHDCodec::Huffman;
    
    m_chdConverter->setCodec(chdCodec);
    
    CHDConversionResult result;
    
    // Determine file type and convert
    QString extension = fileInfo.suffix().toLower();
    if (extension == "cue") {
        result = m_chdConverter->convertCueToCHD(path);
    } else if (extension == "iso") {
        result = m_chdConverter->convertIsoToCHD(path);
    } else if (extension == "gdi") {
        result = m_chdConverter->convertGdiToCHD(path);
    } else {
        emit conversionError("Unsupported file format: " + extension);
        m_converting = false;
        emit convertingChanged();
        return;
    }
    
    m_converting = false;
    emit convertingChanged();
    
    if (result.success) {
        qDebug() << "CHD conversion successful:" << result.outputPath;
        emit conversionCompleted(result.outputPath);
    } else {
        qDebug() << "CHD conversion failed:" << result.error;
        emit conversionError(result.error);
    }
}

void ConversionController::extractCHD(const QString &path)
{
    if (m_converting) {
        qWarning() << "Conversion already in progress";
        return;
    }
    
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        emit conversionError("File not found: " + path);
        return;
    }
    
    if (fileInfo.suffix().toLower() != "chd") {
        emit conversionError("Not a CHD file: " + path);
        return;
    }
    
    m_converting = true;
    emit convertingChanged();
    
    qDebug() << "Extracting CHD:" << path;
    
    CHDConversionResult result = m_chdConverter->extractCHDToCue(path);
    
    m_converting = false;
    emit convertingChanged();
    
    if (result.success) {
        qDebug() << "CHD extraction successful:" << result.outputPath;
        emit conversionCompleted(result.outputPath);
    } else {
        qDebug() << "CHD extraction failed:" << result.error;
        emit conversionError(result.error);
    }
}

void ConversionController::extractArchive(const QString &path)
{
    if (m_converting) {
        qWarning() << "Conversion already in progress";
        return;
    }
    
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        emit conversionError("File not found: " + path);
        return;
    }
    
    if (!m_archiveExtractor->canExtract(path)) {
        emit conversionError("Unsupported archive format or extraction tool not available");
        return;
    }
    
    m_converting = true;
    emit convertingChanged();
    
    qDebug() << "Extracting archive:" << path;
    
    // Extract to same directory as archive
    QString outputDir = fileInfo.absolutePath();
    ExtractionResult result = m_archiveExtractor->extract(path, outputDir, true);
    
    m_converting = false;
    emit convertingChanged();
    
    if (result.success) {
        qDebug() << "Archive extraction successful:" << result.filesExtracted << "files extracted";
        
        // Update database with extracted file paths
        if (m_db) {
            QList<FileRecord> files = m_db->getAllFiles();
            for (const FileRecord &file : files) {
                // Check if this file came from this archive
                if (file.currentPath == path || file.originalPath.contains(path)) {
                    QString extractedPath = outputDir + "/" + file.filename;
                    QFileInfo extractedInfo(extractedPath);
                    if (extractedInfo.exists()) {
                        qDebug() << "Updating file path:" << file.filename << "to" << extractedPath;
                        m_db->updateFilePath(file.id, extractedPath);
                    }
                }
            }
        }
        
        emit conversionCompleted(outputDir);
    } else {
        qDebug() << "Archive extraction failed:" << result.error;
        emit conversionError(result.error);
    }
}

} // namespace Remus
