#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include "metadata/artwork_downloader.h"

using namespace Remus;

class ArtworkDownloaderTest : public QObject {
    Q_OBJECT

private slots:
    void downloadsLocalFile();
    void returnsCorrectedFormatPath();
    void invalidUrlFails();
    void httpUrlFails();
};

void ArtworkDownloaderTest::downloadsLocalFile()
{
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

void ArtworkDownloaderTest::returnsCorrectedFormatPath()
{
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

void ArtworkDownloaderTest::invalidUrlFails()
{
    ArtworkDownloader downloader;
    QSignalSpy failSpy(&downloader, &ArtworkDownloader::downloadFailed);
    const bool ok = downloader.download(QUrl("http://"), "/tmp/nowhere.bin");
    QVERIFY(!ok);
    QVERIFY(!failSpy.isEmpty());
}

void ArtworkDownloaderTest::httpUrlFails()
{
    ArtworkDownloader downloader;
    QSignalSpy failSpy(&downloader, &ArtworkDownloader::downloadFailed);

    const bool ok = downloader.download(QUrl(QStringLiteral("http://example.com/cover.jpg")),
                                        QStringLiteral("/tmp/nowhere.bin"));
    QVERIFY(!ok);
    QCOMPARE(failSpy.count(), 1);
}

QTEST_MAIN(ArtworkDownloaderTest)
#include "test_artwork_downloader.moc"
