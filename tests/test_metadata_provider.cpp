#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QUrl>
#include "metadata/metadata_provider.h"

using namespace Remus;

class DummyProvider : public MetadataProvider {
    Q_OBJECT
public:
    using MetadataProvider::MetadataProvider;
    QString name() const override { return QStringLiteral("dummy"); }
    bool requiresAuth() const override { return false; }
    QList<SearchResult> searchByName(const QString &, const QString &, const QString &) override { return {}; }
    GameMetadata getByHash(const QString &, const QString &) override { return {}; }
    GameMetadata getById(const QString &) override { return {}; }
    ArtworkUrls getArtwork(const QString &) override { return {}; }
};

class MetadataProviderTest : public QObject {
    Q_OBJECT

private slots:
    void credentialsMarkAuthenticated();
    void downloadImageReadsLocalFile();
};

void MetadataProviderTest::credentialsMarkAuthenticated()
{
    DummyProvider provider;
    provider.setCredentials("user", "pass");
    QVERIFY(provider.isAvailable());
}

void MetadataProviderTest::downloadImageReadsLocalFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString filePath = dir.filePath("image.bin");
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray payload("remus-data");
    QVERIFY(file.write(payload) == payload.size());
    file.close();

    DummyProvider provider;
    const QByteArray result = provider.downloadImage(QUrl::fromLocalFile(filePath));

    QCOMPARE(result, payload);
}

QTEST_MAIN(MetadataProviderTest)
#include "test_metadata_provider.moc"
