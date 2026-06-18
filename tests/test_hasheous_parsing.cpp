#include <QtTest>
#include "metadata/hasheous_provider.h"

using namespace Remus;

class MockHasheousProvider : public HasheousProvider {
    Q_OBJECT
public:
    MockHasheousProvider(
        const QJsonObject &gameFixture, const QMap<int, QJsonObject> &companyFixtures, QObject *parent = nullptr)
        : HasheousProvider(parent)
        , m_gameFixture(gameFixture)
        , m_companyFixtures(companyFixtures) { }

    GameMetadata callFetchIgdbMetadata(int igdbId) {
        return fetchIgdbMetadata(igdbId);
    }

protected:
    bool metadataProxyEnabled() const override {
        return true;
    }

    QJsonObject makeRequest(const QString &endpoint, const QUrlQuery &params) override {
        if (endpoint.contains("/MetadataProxy/IGDB/Game")) {
            return m_gameFixture;
        }
        if (endpoint.contains("/MetadataProxy/IGDB/Company")) {
            bool ok = false;
            int id = params.queryItemValue("Id").toInt(&ok);
            if (ok && m_companyFixtures.contains(id)) {
                return m_companyFixtures.value(id);
            }
        }
        return QJsonObject();
    }

private:
    QJsonObject m_gameFixture;
    QMap<int, QJsonObject> m_companyFixtures;
};

class HasheousParsingTest : public QObject {
    Q_OBJECT
private slots:
    void parseIsoDateGenresCompaniesScreenshotsSystem();
    void parseTimestampAndArrayGenres();
    void getByHashesKeepsHashMatchWhenMetadataProxyDisabled();
    void lookupSendsArrayPayloadWithCamelCaseKeys();
    void chdHashEntrySendsSha1Only();
    void emptyHashEntriesSkipLookup();
    void multipleHashEntriesSendArrayPayload();
};

void HasheousParsingTest::parseIsoDateGenresCompaniesScreenshotsSystem() {
    const int igdbId = 123;

    QJsonObject game;
    game["name"] = "Sonic the Hedgehog";
    game["summary"] = "Blue blur";
    game["first_release_date"] = "1991-06-23T00:00:00+00:00";

    QJsonObject genres;
    genres["8"] = QJsonObject { { "name", "Platform" } };
    game["genres"] = genres;

    QJsonObject cover { { "url", "//images.igdb.com/igdb/image/upload/t_thumb/cover.jpg" } };
    game["cover"] = cover;

    QJsonObject screenshots;
    screenshots["1"] = QJsonObject { { "url", "//images.igdb.com/igdb/image/upload/t_thumb/screen1.jpg" } };
    screenshots["2"] = QJsonObject { { "url", "//images.igdb.com/igdb/image/upload/t_thumb/screen2.jpg" } };
    game["screenshots"] = screenshots;

    QJsonObject platforms;
    platforms["30"] = QJsonObject { { "slug", "genesis" } };
    game["platforms"] = platforms;

    QJsonObject companies;
    companies["1"] = QJsonObject { { "company", 395 }, { "developer", false }, { "publisher", false } };
    companies["2"] = QJsonObject { { "company", 112 }, { "developer", false }, { "publisher", false } };
    game["involved_companies"] = companies;

    QMap<int, QJsonObject> companyFixtures;
    companyFixtures[395] = QJsonObject { { "name", "Sonic Team" }, { "developed", QJsonArray { igdbId } },
        { "published", QJsonArray { } } };
    companyFixtures[112]
        = QJsonObject { { "name", "Sega" }, { "developed", QJsonArray { } }, { "published", QJsonArray { igdbId } } };

    MockHasheousProvider provider(game, companyFixtures);
    GameMetadata md = provider.callFetchIgdbMetadata(igdbId);

    QCOMPARE(md.title, QString("Sonic the Hedgehog"));
    QCOMPARE(md.releaseDate, QString("1991-06-23"));
    QCOMPARE(md.genres, QStringList({ "Platform" }));
    QCOMPARE(md.boxArtUrl, QString("https://images.igdb.com/igdb/image/upload/t_1080p/cover.jpg"));
    QCOMPARE(md.screenshotUrls.size(), 2);
    QCOMPARE(md.screenshotUrls.at(0), QString("https://images.igdb.com/igdb/image/upload/t_1080p/screen1.jpg"));
    QCOMPARE(md.system, QString("Genesis"));
    QCOMPARE(md.developer, QString("Sonic Team"));
    QCOMPARE(md.publisher, QString("Sega"));
}

void HasheousParsingTest::parseTimestampAndArrayGenres() {
    const int igdbId = 999;

    QJsonObject game;
    game["name"] = "Test Game";
    game["summary"] = "Summary";
    game["first_release_date"] = 677635200; // 1991-06-23 epoch (UTC seconds)

    QJsonArray genresArr;
    genresArr.append(QJsonObject { { "name", "Adventure" } });
    genresArr.append(QJsonObject { { "name", "Puzzle" } });
    game["genres"] = genresArr;

    MockHasheousProvider provider(game, { });
    GameMetadata md = provider.callFetchIgdbMetadata(igdbId);

    QCOMPARE(md.releaseDate, QString("1991-06-23"));
    QCOMPARE(md.genres, QStringList({ "Adventure", "Puzzle" }));
}

namespace {
class HashLookupHasheousProvider : public HasheousProvider {
public:
    int fetchCalls = 0;
    QJsonDocument lastPayload;

protected:
    bool metadataProxyEnabled() const override {
        return false;
    }

    QJsonObject makePostRequest(const QString &, const QJsonDocument &body, const QUrlQuery &) override {
        lastPayload = body;

        QJsonObject metadataEntry;
        metadataEntry["source"] = "IGDB";
        metadataEntry["immutableId"] = "3192";

        QJsonArray metadataArray;
        metadataArray.append(metadataEntry);

        return QJsonObject { { "id", 1 }, { "name", "Sonic the Hedgehog" }, { "metadata", metadataArray },
            { "signatures", QJsonArray() }, { "attributes", QJsonArray() } };
    }

    GameMetadata fetchIgdbMetadata(int igdbId) override {
        Q_UNUSED(igdbId);
        fetchCalls++;
        return GameMetadata();
    }
};
}

void HasheousParsingTest::getByHashesKeepsHashMatchWhenMetadataProxyDisabled() {
    HashLookupHasheousProvider provider;

    GameMetadata md = provider.getByHashes(
        "f9394e97", "1bc674be034e43c96b86487ac69d9293", "6ddb7de1e17e7f6cdb88927bd906352030daa194", "Genesis");

    QCOMPARE(md.title, QString("Sonic the Hedgehog"));
    QCOMPARE(md.providerId, QStringLiteral("hasheous"));
    QCOMPARE(provider.fetchCalls, 0);
    QVERIFY(provider.lastPayload.isArray());
    QCOMPARE(provider.lastPayload.array().size(), 1);
}

void HasheousParsingTest::lookupSendsArrayPayloadWithCamelCaseKeys() {
    HashLookupHasheousProvider provider;

    provider.getByHashes(
        "F9394E97", "1BC674BE034E43C96B86487AC69D9293", "6DDB7DE1E17E7F6CDB88927BD906352030DAA194", "Genesis",
        "aabbccdd001122334455667788990011aabbccdd001122334455667788990011");

    QVERIFY(provider.lastPayload.isArray());
    const QJsonObject entry = provider.lastPayload.array().first().toObject();
    QCOMPARE(entry.value(QStringLiteral("crc")).toString(), QStringLiteral("f9394e97"));
    QCOMPARE(entry.value(QStringLiteral("mD5")).toString(), QStringLiteral("1bc674be034e43c96b86487ac69d9293"));
    QCOMPARE(entry.value(QStringLiteral("shA1")).toString(),
        QStringLiteral("6ddb7de1e17e7f6cdb88927bd906352030daa194"));
    QCOMPARE(entry.value(QStringLiteral("sha256")).toString(),
        QStringLiteral("aabbccdd001122334455667788990011aabbccdd001122334455667788990011"));
}

void HasheousParsingTest::chdHashEntrySendsSha1Only() {
    HashLookupHasheousProvider provider;

    HasheousHashEntry entry;
    entry.crc32 = QStringLiteral("deadbeef");
    entry.md5 = QStringLiteral("1bc674be034e43c96b86487ac69d9293");
    entry.chdSha1 = QStringLiteral("6DDB7DE1E17E7F6CDB88927BD906352030DAA194");
    provider.getByHashEntries({ entry });

    QVERIFY(provider.lastPayload.isArray());
    const QJsonObject payload = provider.lastPayload.array().first().toObject();
    QCOMPARE(payload.size(), 1);
    QCOMPARE(payload.value(QStringLiteral("shA1")).toString(),
        QStringLiteral("6ddb7de1e17e7f6cdb88927bd906352030daa194"));
    QVERIFY(!payload.contains(QStringLiteral("mD5")));
    QVERIFY(!payload.contains(QStringLiteral("crc")));
}

void HasheousParsingTest::emptyHashEntriesSkipLookup() {
    HashLookupHasheousProvider provider;

    const GameMetadata md = provider.getByHashEntries({ HasheousHashEntry { } });

    QVERIFY(md.title.isEmpty());
    QVERIFY(provider.lastPayload.isNull() || provider.lastPayload.isEmpty());
}

void HasheousParsingTest::multipleHashEntriesSendArrayPayload() {
    HashLookupHasheousProvider provider;

    HasheousHashEntry crcOnly;
    crcOnly.crc32 = QStringLiteral("f9394e97");
    HasheousHashEntry md5Only;
    md5Only.md5 = QStringLiteral("1bc674be034e43c96b86487ac69d9293");

    provider.getByHashEntries({ crcOnly, md5Only });

    QVERIFY(provider.lastPayload.isArray());
    QCOMPARE(provider.lastPayload.array().size(), 2);
    QCOMPARE(provider.lastPayload.array().at(0).toObject().keys().size(), 1);
    QCOMPARE(provider.lastPayload.array().at(1).toObject().keys().size(), 1);
}

QTEST_MAIN(HasheousParsingTest)
#include "test_hasheous_parsing.moc"
