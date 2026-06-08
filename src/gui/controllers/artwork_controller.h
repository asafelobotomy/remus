#pragma once

#include <QObject>
#include <QUrl>

#include "../../metadata/artwork_downloader.h"

namespace Remus {

class AppController;

class ArtworkController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool downloading READ isDownloading NOTIFY downloadingChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY progressChanged)
    Q_PROPERTY(int downloadTotal READ downloadTotal NOTIFY progressChanged)
    Q_PROPERTY(QString progressMessage READ progressMessage NOTIFY progressMessageChanged)
    Q_PROPERTY(QUrl previewUrl READ previewUrl NOTIFY previewChanged)
    Q_PROPERTY(QString localArtworkPath READ localArtworkPath NOTIFY previewChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit ArtworkController(AppController *appController, QObject *parent = nullptr);

    bool isDownloading() const {
        return m_downloading;
    }
    int downloadProgress() const {
        return m_downloadProgress;
    }
    int downloadTotal() const {
        return m_downloadTotal;
    }
    QString progressMessage() const {
        return m_progressMessage;
    }
    QUrl previewUrl() const {
        return m_previewUrl;
    }
    QString localArtworkPath() const {
        return m_localArtworkPath;
    }
    QString lastError() const {
        return m_lastError;
    }

    Q_INVOKABLE void refreshSelectedArtwork();
    Q_INVOKABLE bool downloadSelected();
    Q_INVOKABLE void downloadAllMatched();
    Q_INVOKABLE void clearArtworkCache();

signals:
    void downloadingChanged();
    void progressChanged();
    void progressMessageChanged();
    void previewChanged();
    void lastErrorChanged();
    void artworkDownloaded();

private:
    bool refreshArtworkForFile(int fileId, bool requireDownloadableUrl, bool requireConfirmed = true);
    QString defaultArtworkDir() const;
    QString artworkPathForFile(int fileId) const;
    void setLastError(const QString &message);

    AppController *m_appController;
    ArtworkDownloader m_downloader;
    bool m_downloading = false;
    bool m_batchDownloading = false; // suppresses previewChanged() during downloadAllMatched()
    int m_downloadProgress = 0;
    int m_downloadTotal = 0;
    QString m_progressMessage;
    QUrl m_previewUrl;
    QString m_localArtworkPath;
    QString m_lastError;
};

} // namespace Remus