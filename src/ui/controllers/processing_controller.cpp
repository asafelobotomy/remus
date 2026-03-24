#include "processing_controller.h"
#include "../../core/constants/systems.h"
#include <QDebug>
#include <QMetaObject>
#include "../../core/logging_categories.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logUi)
#define qInfo() qCInfo(logUi)
#define qWarning() qCWarning(logUi)
#define qCritical() qCCritical(logUi)

namespace Remus {

using namespace Constants;

ProcessingController::ProcessingController(Database *db, 
                                           ProviderOrchestrator *orchestrator,
                                           QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_orchestrator(orchestrator)
{
    m_hasher = new Hasher(this);
    m_archiveExtractor = new ArchiveExtractor(this);
    m_chdConverter = new CHDConverter(this);
    m_artworkDownloader = new ArtworkDownloader(this);
    
    // Timer for async step transitions
    m_stepTimer = new QTimer(this);
    m_stepTimer->setSingleShot(true);
    connect(m_stepTimer, &QTimer::timeout, this, [this]() {
        advanceStep();
    });
    
    qDebug() << "ProcessingController initialized";
}

ProcessingController::~ProcessingController()
{
    cancelProcessing();
}

double ProcessingController::overallProgress() const
{
    if (m_totalFiles == 0) return 0.0;
    
    // Calculate progress based on completed files + current step progress
    double fileProgress = static_cast<double>(m_currentFileIndex) / m_totalFiles;
    
    // Add fractional progress for current file based on step
    double stepProgress = 0.0;
    switch (m_currentStep) {
        case PipelineStep::Idle: stepProgress = 0.0; break;
        case PipelineStep::Extract: stepProgress = 0.1; break;
        case PipelineStep::Hash: stepProgress = 0.3; break;
        case PipelineStep::Match: stepProgress = 0.5; break;
        case PipelineStep::Metadata: stepProgress = 0.7; break;
        case PipelineStep::Artwork: stepProgress = 0.85; break;
        case PipelineStep::Convert: stepProgress = 0.95; break;
        case PipelineStep::Complete: stepProgress = 1.0; break;
    }
    
    double currentFileContribution = stepProgress / m_totalFiles;
    return fileProgress + currentFileContribution;
}

QString ProcessingController::currentStep() const
{
    switch (m_currentStep) {
        case PipelineStep::Idle: return "Idle";
        case PipelineStep::Extract: return "Extracting";
        case PipelineStep::Hash: return "Hashing";
        case PipelineStep::Match: return "Matching";
        case PipelineStep::Metadata: return "Fetching Metadata";
        case PipelineStep::Artwork: return "Downloading Artwork";
        case PipelineStep::Convert: return "Converting to CHD";
        case PipelineStep::Complete: return "Complete";
    }
    return "Unknown";
}

void ProcessingController::setConvertToChd(bool enabled)
{
    if (m_convertToChd != enabled) {
        m_convertToChd = enabled;
        emit optionsChanged();
    }
}

void ProcessingController::setDownloadArtwork(bool enabled)
{
    if (m_downloadArtwork != enabled) {
        m_downloadArtwork = enabled;
        emit optionsChanged();
    }
}

void ProcessingController::setFetchMetadata(bool enabled)
{
    if (m_fetchMetadata != enabled) {
        m_fetchMetadata = enabled;
        emit optionsChanged();
    }
}

void ProcessingController::setArtworkBasePath(const QString &path)
{
    if (m_artworkBasePath != path) {
        m_artworkBasePath = path;
        emit artworkBasePathChanged();
    }
}

void ProcessingController::startProcessing(const QVariantList &fileIds)
{
    if (m_processing) {
        qWarning() << "Processing already in progress";
        return;
    }
    
    if (fileIds.isEmpty()) {
        qWarning() << "No files to process";
        return;
    }
    
    // Build queue
    m_fileQueue.clear();
    for (const QVariant &v : fileIds) {
        int fileId = v.toInt();
        if (fileId > 0) {
            m_fileQueue.append(fileId);
        }
    }
    
    if (m_fileQueue.isEmpty()) {
        qWarning() << "No valid file IDs in queue";
        return;
    }
    
    // Initialize state
    m_processing = true;
    m_paused = false;
    m_cancelled = false;
    m_currentFileIndex = 0;
    m_totalFiles = m_fileQueue.size();
    m_successCount = 0;
    m_failCount = 0;
    
    emit processingChanged();
    emit progressChanged();
    emit processingStarted(m_totalFiles);
    
    qInfo() << "Starting processing pipeline for" << m_totalFiles << "files";
    qInfo() << "Options: CHD=" << m_convertToChd 
            << ", Artwork=" << m_downloadArtwork 
            << ", Metadata=" << m_fetchMetadata;
    
    // Start processing first file
    processNextFile();
}

void ProcessingController::pauseProcessing()
{
    if (!m_processing || m_paused) return;
    
    m_paused = true;
    emit pausedChanged();
    setStatusMessage("Paused");
    qInfo() << "Processing paused";
}

void ProcessingController::resumeProcessing()
{
    if (!m_processing || !m_paused) return;
    
    m_paused = false;
    emit pausedChanged();
    qInfo() << "Processing resumed";
    
    // Continue with current step
    advanceStep();
}

void ProcessingController::cancelProcessing()
{
    if (!m_processing) return;
    
    m_cancelled = true;
    m_processing = false;
    m_paused = false;
    m_currentStep = PipelineStep::Idle;
    
    emit processingChanged();
    emit pausedChanged();
    emit currentStepChanged();
    emit processingCancelled();
    
    setStatusMessage("Cancelled");
    qInfo() << "Processing cancelled. Completed:" << m_successCount << "Failed:" << m_failCount;
}

QVariantList ProcessingController::getPendingFiles() const
{
    QVariantList result;
    for (int i = m_currentFileIndex; i < m_fileQueue.size(); i++) {
        result.append(m_fileQueue[i]);
    }
    return result;
}

QVariantMap ProcessingController::getProcessingStats() const
{
    QVariantMap stats;
    stats["total"] = m_totalFiles;
    stats["completed"] = m_currentFileIndex;
    stats["success"] = m_successCount;
    stats["failed"] = m_failCount;
    stats["pending"] = m_fileQueue.size() - m_currentFileIndex;
    stats["progress"] = overallProgress();
    return stats;
}

void ProcessingController::processNextFile()
{
    if (m_cancelled) return;
    
    // Check if we've processed all files
    if (m_currentFileIndex >= m_fileQueue.size()) {
        // All done!
        m_processing = false;
        m_currentStep = PipelineStep::Idle;
        
        emit processingChanged();
        emit currentStepChanged();
        emit processingCompleted(m_successCount, m_failCount);
        emit libraryUpdated();
        
        setStatusMessage(QString("Complete: %1 processed, %2 failed")
                        .arg(m_successCount).arg(m_failCount));
        qInfo() << "Processing complete. Success:" << m_successCount << "Failed:" << m_failCount;
        return;
    }
    
    // Get next file
    m_currentFileId = m_fileQueue[m_currentFileIndex];
    
    // Get file info from database
    FileRecord file = m_db->getFileById(m_currentFileId);
    if (file.id <= 0) {
        qWarning() << "File not found in database:" << m_currentFileId;
        completeCurrentFile(false, "File not found in database");
        return;
    }
    
    m_currentFilename = file.filename;
    m_currentFilePath = file.currentPath;
    m_currentSystemId = file.systemId;
    m_workingFilePath = file.currentPath;
    m_extractedDir.clear();
    m_wasArchive = false;
    
    emit currentFileChanged();
    emit fileStarted(m_currentFileId, m_currentFilename);
    
    qDebug() << "Processing file" << (m_currentFileIndex + 1) << "/" << m_totalFiles
             << ":" << m_currentFilename;
    
    // Determine starting step based on file type
    if (isArchiveFile(m_currentFilePath)) {
        m_currentStep = PipelineStep::Extract;
    } else {
        m_currentStep = PipelineStep::Hash;
    }
    
    emit currentStepChanged();
    executeStep(m_currentStep);
}

void ProcessingController::executeStep(PipelineStep step)
{
    if (m_cancelled) return;
    if (m_paused) return;
    
    emit stepStarted(m_currentFileId, currentStep());
    setStatusMessage(QString("%1: %2").arg(currentStep()).arg(m_currentFilename));
    
    switch (step) {
        case PipelineStep::Extract:
            stepExtract();
            break;
        case PipelineStep::Hash:
            stepHash();
            break;
        case PipelineStep::Match:
            stepMatch();
            break;
        case PipelineStep::Metadata:
            stepMetadata();
            break;
        case PipelineStep::Artwork:
            stepArtwork();
            break;
        case PipelineStep::Convert:
            stepConvert();
            break;
        case PipelineStep::Complete:
            completeCurrentFile(true);
            break;
        case PipelineStep::Idle:
            break;
    }
}

void ProcessingController::advanceStep()
{
    if (m_cancelled || m_paused) return;
    
    // Determine next step
    PipelineStep nextStep = PipelineStep::Complete;
    
    switch (m_currentStep) {
        case PipelineStep::Extract:
            nextStep = PipelineStep::Hash;
            break;
        case PipelineStep::Hash:
            nextStep = PipelineStep::Match;
            break;
        case PipelineStep::Match:
            nextStep = m_fetchMetadata ? PipelineStep::Metadata : 
                       (m_downloadArtwork ? PipelineStep::Artwork :
                        (m_convertToChd && isDiscBasedSystem(m_currentSystemId) ? 
                         PipelineStep::Convert : PipelineStep::Complete));
            break;
        case PipelineStep::Metadata:
            nextStep = m_downloadArtwork ? PipelineStep::Artwork :
                       (m_convertToChd && isDiscBasedSystem(m_currentSystemId) ? 
                        PipelineStep::Convert : PipelineStep::Complete);
            break;
        case PipelineStep::Artwork:
            nextStep = (m_convertToChd && isDiscBasedSystem(m_currentSystemId)) ? 
                       PipelineStep::Convert : PipelineStep::Complete;
            break;
        case PipelineStep::Convert:
            nextStep = PipelineStep::Complete;
            break;
        case PipelineStep::Complete:
        case PipelineStep::Idle:
            return;
    }
    
    emit stepCompleted(m_currentFileId, currentStep(), true);
    
    m_currentStep = nextStep;
    emit currentStepChanged();
    emit progressChanged();
    
    executeStep(m_currentStep);
}

void ProcessingController::completeCurrentFile(bool success, const QString &error)
{
    if (success) {
        // Mark file as processed in database
        m_db->markFileProcessed(m_currentFileId, "processed");
        m_successCount++;
        qDebug() << "File processed successfully:" << m_currentFilename;
        
        // Create .remus.md marker file in the extracted directory
        createMarkerFile(m_currentFileId);
        
        // Move original archive to Originals folder
        if (m_wasArchive && !m_currentFilePath.isEmpty()) {
            moveArchiveToOriginals(m_currentFilePath);
        }
    } else {
        m_db->markFileProcessed(m_currentFileId, "failed");
        m_failCount++;
        qWarning() << "File processing failed:" << m_currentFilename << "-" << error;
        emit processingError(m_currentFileId, currentStep(), error);
    }
    
    emit fileCompleted(m_currentFileId, success, error);
    emit progressChanged();
    
    // Move to next file
    m_currentFileIndex++;
    m_currentStep = PipelineStep::Idle;
    
    // Small delay before next file to allow UI updates
    QMetaObject::invokeMethod(this, "processNextFile", Qt::QueuedConnection);
}

void ProcessingController::onStepComplete(bool success, const QString &error)
{
    if (!success) {
        completeCurrentFile(false, error);
        return;
    }
    
    // Schedule next step
    m_stepTimer->start(10);
}

} // namespace Remus
