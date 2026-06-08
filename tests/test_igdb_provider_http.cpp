#include <QtTest/QtTest>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include "../src/core/constants/providers.h"
#include "../src/metadata/igdb_provider.h"

using namespace Remus;

namespace {

class BufferReply : public QNetworkReply {
    Q_OBJECT

public:
    explicit BufferReply(const QByteArray &payload, QObject *parent = nullptr)
        : QNetworkReply(parent)
        , m_payload(payload) {
        open(QIODevice::ReadOnly);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        QMetaObject::invokeMethod(this, [this]() { emit finished(); }, Qt::QueuedConnection);
    }

    void abort() override { }

protected:
    qint64 readData(char *data, qint64 maxSize) override {
        if (m_offset >= m_payload.size()) {
            return 0;
        }
        const qint64 len = qMin(maxSize, m_payload.size() - m_offset);
        memcpy(data, m_payload.constData() + m_offset, static_cast<size_t>(len));
        m_offset += len;
        return len;
    }

private:
    QByteArray m_payload;
    qint64 m_offset = 0;
};

class IgdbFakeNetworkAccessManager : public QNetworkAccessManager {
    Q_OBJECT

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override {
        const QString url = request.url().toString();
        if (url.contains(QStringLiteral("oauth2/token"))) {
            return new BufferReply(QByteArray(R"({"access_token":"test-token","expires_in":3600})"), this);
        }
        if (url.contains(QStringLiteral("api.igdb.com"))) {
            return new BufferReply(
                QByteArray(
                    R"([{"id":42,"name":"Test Game","summary":"A test summary","first_release_date":631152000}])"),
                this);
        }
        return new BufferReply(QByteArray("[]"), this);
    }
};

class TestIGDBProvider : public IGDBProvider {
public:
    explicit TestIGDBProvider(QNetworkAccessManager *networkManager, QObject *parent = nullptr)
        : IGDBProvider(parent) {
        delete m_networkManager;
        m_networkManager = networkManager;
    }
};

} // namespace

class IGDBProviderHttpTest : public QObject {
    Q_OBJECT

private slots:
    void getByIdParsesGameJsonFromHttpResponse();
    void searchByNameParsesResultsFromHttpResponse();
};

void IGDBProviderHttpTest::getByIdParsesGameJsonFromHttpResponse() {
    IgdbFakeNetworkAccessManager networkManager;
    TestIGDBProvider provider(&networkManager);
    provider.setCredentials(QStringLiteral("client-id"), QStringLiteral("client-secret"));

    const GameMetadata metadata = provider.getById(QStringLiteral("42"));

    QCOMPARE(metadata.id, QStringLiteral("42"));
    QCOMPARE(metadata.title, QStringLiteral("Test Game"));
    QCOMPARE(metadata.description, QStringLiteral("A test summary"));
    QCOMPARE(metadata.providerId, Constants::Providers::IGDB);
    QVERIFY(!metadata.releaseDate.isEmpty());
}

void IGDBProviderHttpTest::searchByNameParsesResultsFromHttpResponse() {
    IgdbFakeNetworkAccessManager networkManager;
    TestIGDBProvider provider(&networkManager);
    provider.setCredentials(QStringLiteral("client-id"), QStringLiteral("client-secret"));

    const QList<SearchResult> results = provider.searchByName(QStringLiteral("Test Game"), QStringLiteral("NES"));

    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().id, QStringLiteral("42"));
    QCOMPARE(results.first().title, QStringLiteral("Test Game"));
    QCOMPARE(results.first().system, QStringLiteral("NES"));
}

QTEST_MAIN(IGDBProviderHttpTest)

#include "test_igdb_provider_http.moc"
