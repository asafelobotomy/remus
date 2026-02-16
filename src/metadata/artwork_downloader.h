#ifndef REMUS_ARTWORK_DOWNLOADER_H
#define REMUS_ARTWORK_DOWNLOADER_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QNetworkAccessManager>

namespace Remus {

/**
 * @brief Downloads and saves artwork files
 */
class ArtworkDownloader : public QObject {
    Q_OBJECT

public:
    explicit ArtworkDownloader(QObject *parent = nullptr);

    /**
     * @brief Download artwork to file
     * @param url Source URL
     * @param destPath Destination file path
     * @return True if successful
     */
    bool download(const QUrl &url, const QString &destPath);

    /**
     * @brief Download artwork to memory
     * @param url Source URL
     * @return Downloaded image data
     */
    QByteArray downloadToMemory(const QUrl &url);

    /**
     * @brief Set maximum parallel downloads
     */
    void setMaxConcurrent(int max) { m_maxConcurrent = max; }

signals:
    void downloadProgress(const QUrl &url, qint64 bytesReceived, qint64 bytesTotal);
    void downloadCompleted(const QUrl &url, const QString &filePath);
    void downloadFailed(const QUrl &url, const QString &error);

private:
    QNetworkAccessManager *m_networkManager;
    int m_maxConcurrent = 4;
    int m_activeDownloads = 0;
};

} // namespace Remus

#endif // REMUS_ARTWORK_DOWNLOADER_H
