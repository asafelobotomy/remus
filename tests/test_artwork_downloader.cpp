#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QImage>
#include <QHostAddress>
#include <QHostInfo>
#include <QNetworkReply>
#include <QSignalSpy>
#include "metadata/artwork_downloader.h"

using namespace Remus;

// ── C2 helpers: fake network reply with closed device ────────────────────────

class ClosedDeviceReply : public QNetworkReply {
    Q_OBJECT
public:
    explicit ClosedDeviceReply(QObject *parent = nullptr)
        : QNetworkReply(parent) {
        // Default error is NoError. Device is never opened, so isOpen() == false.
        QMetaObject::invokeMethod(this, [this]() { emit finished(); }, Qt::QueuedConnection);
    }
    void abort() override { }

protected:
    qint64 readData(char *, qint64) override {
        return -1;
    }
};

class FakeNetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT
public:
    explicit FakeNetworkAccessManager(QObject *parent = nullptr)
        : QNetworkAccessManager(parent) { }

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &, QIODevice *) override {
        return new ClosedDeviceReply(this);
    }
};

// ── Finding #4 helpers: fake redirect to private IP ─────────────────────────

class RedirectReply : public QNetworkReply {
    Q_OBJECT
public:
    explicit RedirectReply(const QUrl &redirectTo, QObject *parent = nullptr)
        : QNetworkReply(parent) {
        // Signal a redirect to the target URL before finishing.
        setAttribute(QNetworkRequest::RedirectionTargetAttribute, redirectTo);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 301);
        QMetaObject::invokeMethod(this, [this]() { emit finished(); }, Qt::QueuedConnection);
    }
    void abort() override { }

protected:
    qint64 readData(char *, qint64) override {
        return -1;
    }
};

class RedirectingFakeManager : public QNetworkAccessManager {
    Q_OBJECT
public:
    explicit RedirectingFakeManager(const QUrl &redirectTo, QObject *parent = nullptr)
        : QNetworkAccessManager(parent)
        , m_redirectTo(redirectTo) { }

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &, QIODevice *) override {
        return new RedirectReply(m_redirectTo, this);
    }

private:
    QUrl m_redirectTo;
};

// ────────────────────────────────────────────────────────────────────────────

class ArtworkDownloaderTest : public QObject {
    Q_OBJECT

private slots:
    void downloadsLocalFile();
    void returnsCorrectedFormatPath();
    void invalidUrlFails();
    void httpUrlFails();
    void fileUrlRejectedAsRemote();
    void localhostAndPrivateHostsRejectedAsRemote();
    void resolvedPrivateAddressesRejected();
    void downloadFromClosedDeviceReturnsEmpty();
    void redirectToPrivateHostRejected();
};

void ArtworkDownloaderTest::downloadsLocalFile() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString source = dir.filePath("source.bin");
    QFile src(source);
    QVERIFY(src.open(QIODevice::WriteOnly));
    QVERIFY(src.write("artwork-bytes") == 13);
    src.close();

    const QString dest = dir.filePath("dest.bin");

    ArtworkDownloader downloader;
    QSignalSpy progressSpy(&downloader, &ArtworkDownloader::downloadProgress);
    QSignalSpy completeSpy(&downloader, &ArtworkDownloader::downloadCompleted);

    const bool ok = downloader.download(QUrl::fromLocalFile(source), dest);
    QVERIFY(ok);
    QVERIFY(QFile::exists(dest));
    QVERIFY(!progressSpy.isEmpty());
    QCOMPARE(completeSpy.count(), 1);
}

void ArtworkDownloaderTest::returnsCorrectedFormatPath() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString source = dir.filePath("source.png");
    QImage image(1, 1, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QVERIFY(image.save(source, "PNG"));

    const QString dest = dir.filePath("cover.jpg");
    QString savedPath;

    ArtworkDownloader downloader;
    const bool ok = downloader.download(QUrl::fromLocalFile(source), dest, &savedPath);
    QVERIFY(ok);
    QCOMPARE(savedPath, dir.filePath("cover.png"));
    QVERIFY(QFile::exists(savedPath));
    QVERIFY(!QFile::exists(dest));
}

void ArtworkDownloaderTest::invalidUrlFails() {
    ArtworkDownloader downloader;
    QSignalSpy failSpy(&downloader, &ArtworkDownloader::downloadFailed);
    const bool ok = downloader.download(QUrl("http://"), "/tmp/nowhere.bin");
    QVERIFY(!ok);
    QVERIFY(!failSpy.isEmpty());
}

void ArtworkDownloaderTest::httpUrlFails() {
    ArtworkDownloader downloader;
    QSignalSpy failSpy(&downloader, &ArtworkDownloader::downloadFailed);

    const bool ok
        = downloader.download(QUrl(QStringLiteral("http://example.com/cover.jpg")), QStringLiteral("/tmp/nowhere.bin"));
    QVERIFY(!ok);
    QCOMPARE(failSpy.count(), 1);
}

void ArtworkDownloaderTest::fileUrlRejectedAsRemote() {
    // file:// URLs must be rejected by isSupportedRemoteUrl so that a malicious
    // provider response cannot coerce local-file reads via the artwork path.
    QVERIFY(!ArtworkDownloader::isSupportedRemoteUrl(QUrl::fromLocalFile(QStringLiteral("/etc/passwd"))));
    QVERIFY(!ArtworkDownloader::isSupportedRemoteUrl(QUrl(QStringLiteral("http://example.com/art.jpg"))));
    QVERIFY(ArtworkDownloader::isSupportedRemoteUrl(QUrl(QStringLiteral("https://media.example.com/cover.jpg"))));
    QVERIFY(!ArtworkDownloader::isSupportedRemoteUrl(QUrl(QStringLiteral("https://"))));
}

void ArtworkDownloaderTest::localhostAndPrivateHostsRejectedAsRemote() {
    QVERIFY(!ArtworkDownloader::isSupportedRemoteUrl(QUrl(QStringLiteral("https://localhost/cover.jpg"))));
    QVERIFY(!ArtworkDownloader::isSupportedRemoteUrl(QUrl(QStringLiteral("https://127.0.0.1/cover.jpg"))));
    QVERIFY(!ArtworkDownloader::isSupportedRemoteUrl(QUrl(QStringLiteral("https://192.168.1.5/cover.jpg"))));
    QVERIFY(!ArtworkDownloader::isSupportedRemoteUrl(QUrl(QStringLiteral("https://[::1]/cover.jpg"))));
    QVERIFY(ArtworkDownloader::isSupportedRemoteUrl(QUrl(QStringLiteral("https://cdn.example.com/cover.jpg"))));
}

void ArtworkDownloaderTest::resolvedPrivateAddressesRejected() {
    QVERIFY(!ArtworkDownloader::areResolvedRemoteAddressesAllowed({ }));
    QVERIFY(!ArtworkDownloader::areResolvedRemoteAddressesAllowed({ QHostAddress(QStringLiteral("127.0.0.1")) }));
    QVERIFY(!ArtworkDownloader::areResolvedRemoteAddressesAllowed({ QHostAddress(QStringLiteral("192.168.1.5")) }));
    QVERIFY(!ArtworkDownloader::areResolvedRemoteAddressesAllowed({ QHostAddress(QStringLiteral("::1")) }));
    QVERIFY(ArtworkDownloader::areResolvedRemoteAddressesAllowed({ QHostAddress(QStringLiteral("93.184.216.34")) }));
}

void ArtworkDownloaderTest::downloadFromClosedDeviceReturnsEmpty() {
    // Exercises the isOpen() guard added for C2. Requires DNS resolution for
    // example.com to pass the SSRF security check; skip in offline environments.
    const QHostInfo hostInfo = QHostInfo::fromName(QStringLiteral("example.com"));
    if (hostInfo.error() != QHostInfo::NoError
        || !ArtworkDownloader::areResolvedRemoteAddressesAllowed(hostInfo.addresses())) {
        QSKIP("DNS resolution for example.com failed or returned a disallowed address");
    }

    FakeNetworkAccessManager mgr;
    ArtworkDownloader downloader(&mgr);

    const QByteArray data = downloader.downloadToMemory(QUrl(QStringLiteral("https://example.com/img.png")));

    QVERIFY(data.isEmpty());
}

void ArtworkDownloaderTest::redirectToPrivateHostRejected() {
    // Finding #4 — A redirect pointing to a private-IP literal must be rejected
    // via the URL check before making another network connection. This exercises
    // the redirect guard path and confirms private-address redirects are blocked.
    //
    // Starting URL is an IP-literal public address (8.8.8.8) so QHostInfo::fromName
    // resolves it without a live DNS lookup. The fake manager then redirects to the
    // private address 192.168.0.1, which isSupportedRemoteUrl must reject.
    //
    // Note: DNS-rebinding (non-IP-literal hostname that resolves to a private address)
    // is covered by the new QHostInfo re-resolution guard but requires DNS injection
    // to test at the unit level; see areResolvedRemoteAddressesAllowed() tests above.
    RedirectingFakeManager mgr(QUrl(QStringLiteral("https://192.168.0.1/pwned.png")));
    ArtworkDownloader downloader(&mgr);
    QSignalSpy failSpy(&downloader, &ArtworkDownloader::downloadFailed);

    // 8.8.8.8 is a public IP — isSupportedRemoteUrl accepts it and
    // QHostInfo::fromName resolves the numeric literal without a network call.
    const QByteArray data = downloader.downloadToMemory(QUrl(QStringLiteral("https://8.8.8.8/cover.png")));

    QVERIFY(data.isEmpty());
    QCOMPARE(failSpy.count(), 1);
}

QTEST_MAIN(ArtworkDownloaderTest)
#include "test_artwork_downloader.moc"
