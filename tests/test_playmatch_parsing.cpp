#include <QtTest>
#include "metadata/playmatch_provider.h"

using namespace Remus;

class MockPlayMatchProvider : public PlayMatchProvider {
    Q_OBJECT
public:
    MockPlayMatchProvider(const QJsonObject &identifyFixture, const QJsonObject &igdbFixture, QObject *parent = nullptr)
        : PlayMatchProvider(parent)
        , m_identifyFixture(identifyFixture)
        , m_igdbFixture(igdbFixture) { }

    GameMetadata callParseIdentify(const QJsonObject &json) const {
        return parseIdentifyResponse(json);
    }

    GameMetadata callParseIgdbGame(const QJsonObject &json, int igdbId) const {
        return parseIgdbGameJson(json, igdbId);
    }

protected:
    QJsonObject makeRequest(const QString &endpoint, const QUrlQuery &params) override {
        if (endpoint.contains(QStringLiteral("/api/identify/ids")))
            return m_identifyFixture;
        if (endpoint.contains(QStringLiteral("/api/igdb/game"))) {
            Q_UNUSED(params);
            return m_igdbFixture;
        }
        return QJsonObject();
    }

private:
    QJsonObject m_identifyFixture;
    QJsonObject m_igdbFixture;
};

class PlayMatchParsingTest : public QObject {
    Q_OBJECT
private slots:
    void parseIdentifyExtractsIgdbIdAndScore();
    void parseIgdbGameMapsCoreFields();
    void identifyBySignalsMergesIgdbMetadata();
};

void PlayMatchParsingTest::parseIdentifyExtractsIgdbIdAndScore() {
    MockPlayMatchProvider provider { QJsonObject(), QJsonObject() };

    QJsonObject identify;
    identify["gameMatchType"] = QStringLiteral("MD5");
    identify["id"] = QStringLiteral("game-uuid");
    QJsonArray external;
    external.append(QJsonObject {
        { QStringLiteral("providerName"), QStringLiteral("IGDB") },
        { QStringLiteral("providerId"), QStringLiteral("358") },
        { QStringLiteral("matchType"), QStringLiteral("Automatic") },
    });
    identify["externalMetadata"] = external;

    const GameMetadata metadata = provider.callParseIdentify(identify);
    QCOMPARE(metadata.matchScore, 1.0f);
    QCOMPARE(metadata.matchMethod, QStringLiteral("hash"));
    QCOMPARE(metadata.externalIds.value(QStringLiteral("igdb")), QStringLiteral("358"));
}

void PlayMatchParsingTest::parseIgdbGameMapsCoreFields() {
    MockPlayMatchProvider provider { QJsonObject(), QJsonObject() };

    QJsonObject igdb;
    igdb["name"] = QStringLiteral("Super Mario Bros.");
    igdb["summary"] = QStringLiteral("Platform classic");
    igdb["first_release_date"] = 495417600;
    igdb["rating"] = 83.5;

    const GameMetadata metadata = provider.callParseIgdbGame(igdb, 358);
    QCOMPARE(metadata.title, QStringLiteral("Super Mario Bros."));
    QCOMPARE(metadata.description, QStringLiteral("Platform classic"));
    QCOMPARE(metadata.releaseDate, QStringLiteral("1985-09-13"));
    QVERIFY(metadata.rating > 8.0f);
    QCOMPARE(metadata.externalIds.value(QStringLiteral("igdb")), QStringLiteral("358"));
}

void PlayMatchParsingTest::identifyBySignalsMergesIgdbMetadata() {
    QJsonObject identify;
    identify["gameMatchType"] = QStringLiteral("FileNameAndSize");
    identify["id"] = QStringLiteral("game-uuid");
    QJsonArray external;
    external.append(QJsonObject {
        { QStringLiteral("providerName"), QStringLiteral("IGDB") },
        { QStringLiteral("providerId"), QStringLiteral("358") },
        { QStringLiteral("matchType"), QStringLiteral("Automatic") },
    });
    identify["externalMetadata"] = external;

    QJsonObject igdb;
    igdb["name"] = QStringLiteral("Super Mario Bros.");
    igdb["summary"] = QStringLiteral("Platform classic");
    igdb["first_release_date"] = 495417600;
    igdb["rating"] = 80.0;

    MockPlayMatchProvider provider { identify, igdb };
    const GameMetadata metadata = provider.identifyBySignals(
        QStringLiteral("Super Mario Bros. (World).nes"), 40976, QString(), QStringLiteral("7e5265ff43047da9bab44b39195d2f44"),
        QString(), QString());

    QCOMPARE(metadata.title, QStringLiteral("Super Mario Bros."));
    QCOMPARE(metadata.matchScore, 0.4f);
    QCOMPARE(metadata.matchMethod, QStringLiteral("filename_size"));
    QCOMPARE(metadata.providerId, QStringLiteral("playmatch"));
}

QTEST_MAIN(PlayMatchParsingTest)
#include "test_playmatch_parsing.moc"
