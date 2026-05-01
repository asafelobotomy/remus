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
    Q_PROPERTY(QUrl previewUrl READ previewUrl NOTIFY previewChanged)
    Q_PROPERTY(QString localArtworkPath READ localArtworkPath NOTIFY previewChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit ArtworkController(AppController *appController, QObject *parent = nullptr);

    bool isDownloading() const { return m_downloading; }
    int downloadProgress() const { return m_downloadProgress; }
    int downloadTotal() const { return m_downloadTotal; }
    QUrl previewUrl() const { return m_previewUrl; }
    QString localArtworkPath() const { return m_localArtworkPath; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void refreshSelectedArtwork();
    Q_INVOKABLE bool downloadSelected();
    Q_INVOKABLE void downloadAllMatched();

signals:
    void downloadingChanged();
    void progressChanged();
    void previewChanged();
    void lastErrorChanged();

private:
    bool refreshArtworkForFile(int fileId, bool requireDownloadableUrl);
    QString defaultArtworkDir() const;
    QString artworkPathForFile(int fileId) const;
    void setLastError(const QString &message);

    AppController *m_appController;
    ArtworkDownloader m_downloader;
    bool m_downloading = false;
    int m_downloadProgress = 0;
    int m_downloadTotal = 0;
    QUrl m_previewUrl;
    QString m_localArtworkPath;
    QString m_lastError;
};

} // namespace Remus