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
    QString name() const override {
        return QStringLiteral("dummy");
    }
    bool requiresAuth() const override {
        return false;
    }
    QList<SearchResult> searchByName(const QString &, const QString &, const QString &) override {
        return { };
    }
    GameMetadata getByHash(const QString &, const QString &) override {
        return { };
    }
    GameMetadata getById(const QString &) override {
        return { };
    }
    ArtworkUrls getArtwork(const QString &) override {
        return { };
    }
};

class MetadataProviderTest : public QObject {
    Q_OBJECT

private slots:
    void credentialsMarkAuthenticated();
};

void MetadataProviderTest::credentialsMarkAuthenticated() {
    DummyProvider provider;
    provider.setCredentials("user", "pass");
    QVERIFY(provider.isAvailable());
}

QTEST_MAIN(MetadataProviderTest)
#include "test_metadata_provider.moc"
