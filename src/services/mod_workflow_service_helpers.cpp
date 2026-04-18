#include "mod_workflow_service.h"

#include <QCryptographicHash>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include "../core/constants/constants.h"

namespace Remus {

QString ModWorkflowService::resolvePatchPath(const QString &patchUrl,
                                            QString &error,
                                            ProgressCallback cb)
{
    if (patchUrl.isEmpty()) {
        error = "Empty patch URL";
        return {};
    }

    const QUrl url(patchUrl);
    if (url.scheme() == QStringLiteral("file")) {
        const QString localPath = url.toLocalFile();
        if (!QFile::exists(localPath)) {
            error = "Local patch file not found: " + localPath;
            return {};
        }
        return localPath;
    }

    if (url.scheme().isEmpty() || url.isRelative()) {
        if (!QFile::exists(patchUrl)) {
            error = "Patch file not found: " + patchUrl;
            return {};
        }
        return patchUrl;
    }

    if (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) {
        return downloadPatch(url, error, cb);
    }

    error = "Unsupported URL scheme: " + url.scheme();
    return {};
}

QString ModWorkflowService::downloadPatch(const QUrl &url,
                                         QString &error,
                                         ProgressCallback cb)
{
    if (cb) cb("downloading", 2);

    if (!m_downloadDir) {
        m_downloadDir = std::make_unique<QTemporaryDir>();
        if (!m_downloadDir->isValid()) {
            error = "Failed to create temp directory for patch download";
            return {};
        }
    }

    QString filename = QFileInfo(url.path()).fileName();
    if (filename.isEmpty()) {
        filename = QStringLiteral("patch.bin");
    }
    const QString destPath = m_downloadDir->path() + QStringLiteral("/") + filename;

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = manager.get(request);
    if (cb) {
        QObject::connect(reply, &QNetworkReply::downloadProgress,
            [&cb](qint64 received, qint64 total) {
                if (total > 0) {
                    const int pct = 2 + static_cast<int>(received * 10 / total);
                    cb("downloading", pct);
                }
            });
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(Constants::Network::ARTWORK_TIMEOUT_MS);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start();
    loop.exec();

    if (!timeout.isActive()) {
        reply->abort();
        reply->deleteLater();
        error = "Patch download timed out: " + url.toString();
        return {};
    }
    timeout.stop();

    if (reply->error() != QNetworkReply::NoError) {
        error = "Patch download failed: " + reply->errorString();
        reply->deleteLater();
        return {};
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();
    if (data.isEmpty()) {
        error = "Downloaded patch file is empty";
        return {};
    }

    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly)) {
        error = "Failed to write downloaded patch: " + destPath;
        return {};
    }
    file.write(data);
    file.close();

    if (cb) cb("downloaded", 12);
    return destPath;
}

bool ModWorkflowService::verifySha1(const QString &filePath, const QString &expectedSha1)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&file);
    const QString actual = QString::fromLatin1(hash.result().toHex());
    return actual.compare(expectedSha1, Qt::CaseInsensitive) == 0;
}

} // namespace Remus