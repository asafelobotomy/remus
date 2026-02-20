#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QTimer>
#include "../../core/database.h"
#include "../../core/hasher.h"
#include "../../core/archive_extractor.h"
#include "../../core/chd_converter.h"
#include "../../core/system_resolver.h"
#include "../../metadata/provider_orchestrator.h"
#include "../../metadata/artwork_downloader.h"

namespace Remus {

/**
 * @brief Pipeline step enumeration
 */
enum class PipelineStep {
    Idle,
    Extract,
    Hash,
    Match,
    Metadata,
    Artwork,
    Convert,
    Complete
};

/**
 * @brief Controller for batch processing pipeline
 * 
 * Orchestrates the full ROM processing pipeline:
 * 1. Extract (if archived)
 * 2. Hash (CRC32, MD5, SHA1)
 * 3. Match (hash-based and name-based)
 * 4. Metadata (from providers)
 * 5. Artwork (download cover art, screenshots)
 * 6. CHD conversion (optional, for disc-based games)
 * 
 * Exposed to QML as a context property.
 */
class ProcessingController : public QObject {
    Q_OBJECT
    
    // Processing state
    Q_PROPERTY(bool processing READ isProcessing NOTIFY processingChanged)
    Q_PROPERTY(bool paused READ isPaused NOTIFY pausedChanged)
    
    // Progress tracking
    Q_PROPERTY(int currentFileIndex READ currentFileIndex NOTIFY progressChanged)
    Q_PROPERTY(int totalFiles READ totalFiles NOTIFY progressChanged)
    Q_PROPERTY(double overallProgress READ overallProgress NOTIFY progressChanged)
    
    // Current file info
    Q_PROPERTY(QString currentFilename READ currentFilename NOTIFY currentFileChanged)
    Q_PROPERTY(QString currentStep READ currentStep NOTIFY currentStepChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    
    // Pipeline options
    Q_PROPERTY(bool convertToChd READ convertToChd WRITE setConvertToChd NOTIFY optionsChanged)
    Q_PROPERTY(bool downloadArtwork READ downloadArtwork WRITE setDownloadArtwork NOTIFY optionsChanged)
    Q_PROPERTY(bool fetchMetadata READ fetchMetadata WRITE setFetchMetadata NOTIFY optionsChanged)

    // Artwork storage path (set by parent before starting pipeline)
    Q_PROPERTY(QString artworkBasePath READ artworkBasePath WRITE setArtworkBasePath NOTIFY artworkBasePathChanged)
    
public:
    explicit ProcessingController(Database *db, 
                                   ProviderOrchestrator *orchestrator,
                                   QObject *parent = nullptr);
    ~ProcessingController();
    
    // State accessors
    bool isProcessing() const { return m_processing; }
    bool isPaused() const { return m_paused; }
    
    // Progress accessors
    int currentFileIndex() const { return m_currentFileIndex; }
    int totalFiles() const { return m_totalFiles; }
    double overallProgress() const;
    
    // Current file accessors
    QString currentFilename() const { return m_currentFilename; }
    QString currentStep() const;
    QString statusMessage() const { return m_statusMessage; }
    
    // Options accessors
    bool convertToChd() const { return m_convertToChd; }
    void setConvertToChd(bool enabled);
    bool downloadArtwork() const { return m_downloadArtwork; }
    void setDownloadArtwork(bool enabled);
    bool fetchMetadata() const { return m_fetchMetadata; }
    void setFetchMetadata(bool enabled);

    QString artworkBasePath() const { return m_artworkBasePath; }
    void setArtworkBasePath(const QString &path);
    
    // Control methods
    Q_INVOKABLE void startProcessing(const QVariantList &fileIds);
    Q_INVOKABLE void pauseProcessing();
    Q_INVOKABLE void resumeProcessing();
    Q_INVOKABLE void cancelProcessing();
    
    // Query methods
    Q_INVOKABLE QVariantList getPendingFiles() const;
    Q_INVOKABLE QVariantMap getProcessingStats() const;
    
signals:
    // State signals
    void processingChanged();
    void pausedChanged();
    void optionsChanged();
    
    // Progress signals
    void progressChanged();
    void currentFileChanged();
    void currentStepChanged();
    void statusMessageChanged();
    
    // Pipeline events
    void processingStarted(int fileCount);
    void fileStarted(int fileId, const QString &filename);
    void stepStarted(int fileId, const QString &step);
    void stepCompleted(int fileId, const QString &step, bool success);
    void fileCompleted(int fileId, bool success, const QString &error);
    void processingCompleted(int successCount, int failCount);
    void processingCancelled();
    
    // Real-time processing results (for sidebar updates)
    void hashCalculated(int fileId, const QString &crc32, const QString &md5, const QString &sha1);
    void matchFound(int fileId, const QString &gameTitle, const QString &publisher,
                    int releaseYear, int confidence, const QString &matchMethod);
    void metadataUpdated(int fileId, const QString &description, const QString &coverArtUrl,
                        const QString &systemLogoUrl, const QString &screenshotUrl,
                        const QString &titleScreenUrl, float rating, const QString &ratingSource);
    void artworkDownloaded(int fileId, int gameId, const QString &localPath);

    // Error signals
    void processingError(int fileId, const QString &step, const QString &error);

    // Refresh signal (for models)
    void libraryUpdated();

    // Option change notification
    void artworkBasePathChanged();

private slots:
    void processNextFile();
    void onStepComplete(bool success, const QString &error);
    
private:
    // Pipeline execution
    void executeStep(PipelineStep step);
    void advanceStep();
    void completeCurrentFile(bool success, const QString &error = QString());
    
    // Individual pipeline steps
    void stepExtract();
    void stepHash();
    void stepMatch();
    void stepMetadata();
    void stepArtwork();
    void stepConvert();
    
    // Helpers
    bool isDiscBasedSystem(int systemId) const;
    bool isArchiveFile(const QString &path) const;
    QString getSystemPreferredHash(int systemId) const;
    QString getSystemNameForId(int systemId) const;
    void setStatusMessage(const QString &msg);
    void createMarkerFile(int fileId);
    void moveArchiveToOriginals(const QString &archivePath);
    static bool hasMarkerFile(const QString &directoryPath);
    
    // Core objects
    Database *m_db;
    ProviderOrchestrator *m_orchestrator;
    Hasher *m_hasher;
    ArchiveExtractor *m_archiveExtractor;
    CHDConverter *m_chdConverter;
    ArtworkDownloader *m_artworkDownloader;
    
    // Processing state
    bool m_processing = false;
    bool m_paused = false;
    bool m_cancelled = false;
    
    // Queue management
    QList<int> m_fileQueue;
    int m_currentFileIndex = 0;
    int m_totalFiles = 0;
    int m_successCount = 0;
    int m_failCount = 0;
    
    // Current file state
    int m_currentFileId = -1;
    QString m_currentFilename;
    QString m_currentFilePath;
    int m_currentSystemId = 0;
    PipelineStep m_currentStep = PipelineStep::Idle;
    QString m_statusMessage;
    
    // Extracted file path (if working with archive)
    QString m_workingFilePath;
    QString m_extractedDir;  // Directory where files were extracted to
    bool m_wasArchive = false;
    
    // Options
    bool m_convertToChd = false;
    bool m_downloadArtwork = true;
    bool m_fetchMetadata = true;

    // Artwork step state — populated during stepMatch, consumed in stepArtwork
    QString m_artworkBasePath;
    QUrl    m_pendingArtworkUrl;
    int     m_pendingArtworkGameId = -1;

    // Timer for async step completion
    QTimer *m_stepTimer;
};

} // namespace Remus
