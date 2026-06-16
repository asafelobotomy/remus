// SPDX-License-Identifier: GPL-3.0-or-later
//
// Optional live credential probes — skipped unless REMUS_RUN_LIVE_CREDENTIAL_TESTS=1
// and the relevant REMUS_* env vars are set (e.g. after `source .env.local`).

#include <QtTest/QtTest>

#include "../src/core/constants/settings.h"
#include "../src/metadata/igdb_provider.h"
#include "../src/metadata/retroachievements_provider.h"
#include "../src/metadata/screenscraper_provider.h"
#include "../src/services/credential_manager.h"

using namespace Remus;
using namespace Remus::Constants;

namespace {

bool liveCredentialTestsEnabled() {
    return qEnvironmentVariableIntValue("REMUS_RUN_LIVE_CREDENTIAL_TESTS") != 0;
}

} // namespace

class CredentialsLiveTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        if (!liveCredentialTestsEnabled()) {
            QSKIP("Set REMUS_RUN_LIVE_CREDENTIAL_TESTS=1 to run live credential probes "
                  "(source .env.local first).");
        }
    }

    void igdbOAuthFromEnv();
    void retroAchievementsApiKeyFromEnv();
    void screenScraperCredentialsFromEnv();
};

void CredentialsLiveTest::igdbOAuthFromEnv() {
    const QString clientId = CredentialManager::get(QString::fromLatin1(Settings::Providers::IGDB_CLIENT_ID));
    const QString clientSecret = CredentialManager::get(QString::fromLatin1(Settings::Providers::IGDB_CLIENT_SECRET));
    if (clientId.isEmpty() || clientSecret.isEmpty())
        QSKIP("IGDB credentials not configured (REMUS_IGDB_CLIENT_ID / REMUS_IGDB_CLIENT_SECRET)");

    IGDBProvider provider;
    provider.setCredentials(clientId, clientSecret);

    const QList<GameMetadata> games = provider.fetchGamesByPlatformSlug(QStringLiteral("nes"), 0, 1);
    QVERIFY2(!games.isEmpty(), "IGDB OAuth or API call failed — check Twitch client credentials");
    QVERIFY(!games.first().title.isEmpty());
}

void CredentialsLiveTest::retroAchievementsApiKeyFromEnv() {
    const QString username
        = CredentialManager::get(QString::fromLatin1(Settings::Providers::RETROACHIEVEMENTS_USERNAME));
    const QString apiKey = CredentialManager::get(QString::fromLatin1(Settings::Providers::RETROACHIEVEMENTS_API_KEY));
    if (username.isEmpty() || apiKey.isEmpty())
        QSKIP("RetroAchievements credentials not configured "
              "(REMUS_RA_USERNAME or REMUS_RA_USER, plus REMUS_RA_API_KEY)");

    RetroAchievementsProvider provider;
    provider.setCredentials(username, apiKey);

    // RA system 7 = NES — lightweight list call that validates the API key.
    const auto entries = provider.fetchGameListBySystemId(7);
    QVERIFY2(!entries.isEmpty(), "RetroAchievements API rejected credentials or returned no NES games");
}

void CredentialsLiveTest::screenScraperCredentialsFromEnv() {
    const QString user = CredentialManager::get(QString::fromLatin1(Settings::Providers::SCREENSCRAPER_USERNAME));
    const QString pass = CredentialManager::get(QString::fromLatin1(Settings::Providers::SCREENSCRAPER_PASSWORD));
    if (user.isEmpty() || pass.isEmpty())
        QSKIP("ScreenScraper credentials not configured (REMUS_SS_USER / REMUS_SS_PASS)");

    ScreenScraperProvider provider;
    provider.setCredentials(user, pass);

    const auto devId = CredentialManager::get(QString::fromLatin1(Settings::Providers::SCREENSCRAPER_DEVID));
    const auto devPass = CredentialManager::get(QString::fromLatin1(Settings::Providers::SCREENSCRAPER_DEVPASSWORD));
    if (!devId.isEmpty() && !devPass.isEmpty())
        provider.setDeveloperCredentials(devId, devPass);

    const QList<SearchResult> results = provider.searchByName(QStringLiteral("Mario"), QStringLiteral("NES"));
    QVERIFY2(!results.isEmpty(), "ScreenScraper auth or search failed — check REMUS_SS_USER / REMUS_SS_PASS");
}

QTEST_MAIN(CredentialsLiveTest)
#include "test_credentials_live.moc"
