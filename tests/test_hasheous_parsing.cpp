#include <QtTest>
#include "metadata/hasheous_provider.h"

using namespace Remus;

class MockHasheousProvider : public HasheousProvider {
    Q_OBJECT
public:
    MockHasheousProvider(const QJsonObject &gameFixture,
                         const QMap<int, QJsonObject> &companyFixtures,
                         QObject *parent = nullptr)
        : HasheousProvider(parent)
        , m_gameFixture(gameFixture)
        , m_companyFixtures(companyFixtures) {}

    GameMetadata callFetchIgdbMetadata(int igdbId) {
        return fetchIgdbMetadata(igdbId);
    }

protected:
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
};

void HasheousParsingTest::parseIsoDateGenresCompaniesScreenshotsSystem()
{
    const int igdbId = 123;

    QJsonObject game;
    game["name"] = "Sonic the Hedgehog";
    game["summary"] = "Blue blur";
    game["first_release_date"] = "1991-06-23T00:00:00+00:00";

    QJsonObject genres;
    genres["8"] = QJsonObject{{"name", "Platform"}};
    game["genres"] = genres;

    QJsonObject cover{{"url", "//images.igdb.com/igdb/image/upload/t_thumb/cover.jpg"}};
    game["cover"] = cover;

    QJsonObject screenshots;
    screenshots["1"] = QJsonObject{{"url", "//images.igdb.com/igdb/image/upload/t_thumb/screen1.jpg"}};
    screenshots["2"] = QJsonObject{{"url", "//images.igdb.com/igdb/image/upload/t_thumb/screen2.jpg"}};
    game["screenshots"] = screenshots;

    QJsonObject platforms;
    platforms["30"] = QJsonObject{{"slug", "genesis"}};
    game["platforms"] = platforms;

    QJsonObject companies;
    companies["1"] = QJsonObject{{"company", 395}, {"developer", false}, {"publisher", false}};
    companies["2"] = QJsonObject{{"company", 112}, {"developer", false}, {"publisher", false}};
    game["involved_companies"] = companies;

    QMap<int, QJsonObject> companyFixtures;
    companyFixtures[395] = QJsonObject{{"name", "Sonic Team"}, {"developed", QJsonArray{igdbId}}, {"published", QJsonArray{}}};
    companyFixtures[112] = QJsonObject{{"name", "Sega"}, {"developed", QJsonArray{}}, {"published", QJsonArray{igdbId}}};

    MockHasheousProvider provider(game, companyFixtures);
    GameMetadata md = provider.callFetchIgdbMetadata(igdbId);

    QCOMPARE(md.title, QString("Sonic the Hedgehog"));
    QCOMPARE(md.releaseDate, QString("1991-06-23"));
    QCOMPARE(md.genres, QStringList({"Platform"}));
    QCOMPARE(md.boxArtUrl, QString("https://images.igdb.com/igdb/image/upload/t_1080p/cover.jpg"));
    QCOMPARE(md.screenshotUrls.size(), 2);
    QCOMPARE(md.screenshotUrls.at(0), QString("https://images.igdb.com/igdb/image/upload/t_1080p/screen1.jpg"));
    QCOMPARE(md.system, QString("Genesis"));
    QCOMPARE(md.developer, QString("Sonic Team"));
    QCOMPARE(md.publisher, QString("Sega"));
}

void HasheousParsingTest::parseTimestampAndArrayGenres()
{
    const int igdbId = 999;

    QJsonObject game;
    game["name"] = "Test Game";
    game["summary"] = "Summary";
    game["first_release_date"] = 677635200; // 1991-06-23 epoch (UTC seconds)

    QJsonArray genresArr;
    genresArr.append(QJsonObject{{"name", "Adventure"}});
    genresArr.append(QJsonObject{{"name", "Puzzle"}});
    game["genres"] = genresArr;

    MockHasheousProvider provider(game, {});
    GameMetadata md = provider.callFetchIgdbMetadata(igdbId);

    QCOMPARE(md.releaseDate, QString("1991-06-23"));
    QCOMPARE(md.genres, QStringList({"Adventure", "Puzzle"}));
}

QTEST_MAIN(HasheousParsingTest)
#include "test_hasheous_parsing.moc"
