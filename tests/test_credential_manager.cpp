// SPDX-License-Identifier: GPL-3.0-or-later
//
// Unit tests for CredentialManager::get() resolution order and
// SecretStore::readWithStatus() BackendError guard.

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QTemporaryDir>

#include "../src/core/constants/settings.h"
#include "../src/services/credential_manager.h"
#include "../src/services/secret_store.h"

using namespace Remus;
using namespace Remus::Constants;

class CredentialManagerTest : public QObject {
    Q_OBJECT

private slots:
    void testJsonTakesPriorityOverEnvVar();
    void testNonexistentJsonFileFallsThrough();
    void testEnvVarResolvesForMappedKey();
    void testAllProviderEnvMappingsResolve();
    void testRaUsernamePrefersRemusRaUsernameOverLegacyAlias();
    void testRaLegacyEnvAliasResolvesWhenCanonicalUnset();
    void testEnrichmentJsonKeysResolve();
    void testEmptyReturnedWhenNothingSet();
    void testBackendErrorDoesNotFallThroughToQSettings();
};

// ── Helpers ────────────────────────────────────────────────────────────────

/// Write a minimal credentials JSON file with one outer/inner key.
static QString writeCredJson(
    const QTemporaryDir &dir, const QString &outer, const QString &inner, const QString &value) {
    const QJsonObject inner_obj { { inner, value } };
    const QJsonObject root { { outer, inner_obj } };
    const QJsonDocument doc(root);
    const QString path = dir.filePath(QStringLiteral("creds.json"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return { };
    f.write(doc.toJson());
    return path;
}

// ── Tests ──────────────────────────────────────────────────────────────────

/// JSON file value must beat a simultaneously-set env var (step 1 > step 2).
void CredentialManagerTest::testJsonTakesPriorityOverEnvVar() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString jsonPath = writeCredJson(dir, "igdb", "client_id", "json_id");
    QVERIFY(!jsonPath.isEmpty());

    qputenv("REMUS_IGDB_CLIENT_ID", "env_id");
    const QString result = CredentialManager::get("igdb/client_id", jsonPath);
    qunsetenv("REMUS_IGDB_CLIENT_ID");

    QCOMPARE(result, QStringLiteral("json_id"));
}

/// When the JSON path points to a non-existent file the lookup must fall
/// through to the next source (env var), not return empty prematurely.
void CredentialManagerTest::testNonexistentJsonFileFallsThrough() {
    const QString missingPath = QStringLiteral("/nonexistent/path/creds.json");
    qputenv("REMUS_IGDB_CLIENT_SECRET", "env_secret");
    const QString result = CredentialManager::get("igdb/client_secret", missingPath);
    qunsetenv("REMUS_IGDB_CLIENT_SECRET");

    QCOMPARE(result, QStringLiteral("env_secret"));
}

/// An env var in the built-in mapping table must be returned as-is (step 2).
void CredentialManagerTest::testEnvVarResolvesForMappedKey() {
    qputenv("REMUS_TGDB_API_KEY", "tgdb_abc123");
    const QString result = CredentialManager::get("thegamesdb/api_key");
    qunsetenv("REMUS_TGDB_API_KEY");

    QCOMPARE(result, QStringLiteral("tgdb_abc123"));
}

/// Every provider env mapping in CredentialManager must round-trip through get().
void CredentialManagerTest::testAllProviderEnvMappingsResolve() {
    struct Mapping {
        const char *settingsKey;
        const char *envVar;
        const char *value;
    };

    static constexpr Mapping mappings[] = {
        { Settings::Providers::SCREENSCRAPER_USERNAME, "REMUS_SS_USER", "ss_user" },
        { Settings::Providers::SCREENSCRAPER_PASSWORD, "REMUS_SS_PASS", "ss_pass" },
        { Settings::Providers::SCREENSCRAPER_DEVID, "REMUS_SS_DEVID", "ss_devid" },
        { Settings::Providers::SCREENSCRAPER_DEVPASSWORD, "REMUS_SS_DEVPASS", "ss_devpass" },
        { Settings::Providers::THEGAMESDB_API_KEY, "REMUS_TGDB_API_KEY", "tgdb_key" },
        { Settings::Providers::IGDB_CLIENT_ID, "REMUS_IGDB_CLIENT_ID", "igdb_id" },
        { Settings::Providers::IGDB_CLIENT_SECRET, "REMUS_IGDB_CLIENT_SECRET", "igdb_secret" },
        { Settings::Providers::HASHEOUS_CLIENT_API_KEY, "REMUS_HASHEOUS_API_KEY", "hasheous_key" },
        { Settings::Providers::RETROACHIEVEMENTS_USERNAME, "REMUS_RA_USERNAME", "ra_user" },
        { Settings::Providers::RETROACHIEVEMENTS_API_KEY, "REMUS_RA_API_KEY", "ra_key" },
        { Settings::Providers::STEAMGRIDDB_API_KEY, "REMUS_SGDB_API_KEY", "sgdb_key" },
    };

    for (const auto &mapping : mappings) {
        qputenv(mapping.envVar, mapping.value);
        const QString result = CredentialManager::get(QString::fromLatin1(mapping.settingsKey));
        qunsetenv(mapping.envVar);
        QCOMPARE(result, QString::fromLatin1(mapping.value));
    }
}

/// REMUS_RA_USERNAME wins when both legacy REMUS_RA_USER and canonical var are set.
void CredentialManagerTest::testRaUsernamePrefersRemusRaUsernameOverLegacyAlias() {
    qputenv("REMUS_RA_USER", "legacy_user");
    qputenv("REMUS_RA_USERNAME", "canonical_user");
    const QString result = CredentialManager::get(QString::fromLatin1(Settings::Providers::RETROACHIEVEMENTS_USERNAME));
    qunsetenv("REMUS_RA_USER");
    qunsetenv("REMUS_RA_USERNAME");

    QCOMPARE(result, QStringLiteral("canonical_user"));
}

/// REMUS_RA_USER is accepted when REMUS_RA_USERNAME is unset (.env.local compatibility).
void CredentialManagerTest::testRaLegacyEnvAliasResolvesWhenCanonicalUnset() {
    qunsetenv("REMUS_RA_USERNAME");
    qputenv("REMUS_RA_USER", "legacy_only_user");
    const QString result = CredentialManager::get(QString::fromLatin1(Settings::Providers::RETROACHIEVEMENTS_USERNAME));
    qunsetenv("REMUS_RA_USER");

    QCOMPARE(result, QStringLiteral("legacy_only_user"));
}

/// enrichment-credentials.json keys used by compendium enrichment must resolve.
void CredentialManagerTest::testEnrichmentJsonKeysResolve() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QJsonObject igdb {
        { QStringLiteral("client_id"), QStringLiteral("json_igdb_id") },
        { QStringLiteral("client_secret"), QStringLiteral("json_igdb_secret") },
    };
    QJsonObject ra {
        { QStringLiteral("username"), QStringLiteral("json_ra_user") },
        { QStringLiteral("api_key"), QStringLiteral("json_ra_key") },
    };
    const QJsonObject root {
        { QStringLiteral("igdb"), igdb },
        { QStringLiteral("retroachievements"), ra },
    };

    const QString path = dir.filePath(QStringLiteral("enrichment-credentials.json"));
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(QJsonDocument(root).toJson());
    }

    QCOMPARE(CredentialManager::get(QStringLiteral("igdb/client_id"), path), QStringLiteral("json_igdb_id"));
    QCOMPARE(CredentialManager::get(QStringLiteral("igdb/client_secret"), path), QStringLiteral("json_igdb_secret"));
    QCOMPARE(
        CredentialManager::get(QStringLiteral("retroachievements/username"), path), QStringLiteral("json_ra_user"));
    QCOMPARE(CredentialManager::get(QStringLiteral("retroachievements/api_key"), path), QStringLiteral("json_ra_key"));
}

/// A key absent from every source must produce an empty string, not a crash.
void CredentialManagerTest::testEmptyReturnedWhenNothingSet() {
    // Use a key that is not in the env-var mapping table and has no JSON /
    // QSettings / keychain value in a clean test run.
    const QString key = QStringLiteral("test_credential_manager/nonexistent");
    const QString result = CredentialManager::get(key);
    QVERIFY(result.isEmpty());
}

/// When the keychain backend returns BackendError, get() MUST return an empty
/// string rather than downgrading silently to plaintext QSettings storage.
/// When the backend is available and returns NotFound, get() correctly falls
/// through to QSettings (expected safe fallback path).
void CredentialManagerTest::testBackendErrorDoesNotFallThroughToQSettings() {
    // Use a key with no env-var mapping so the lookup reaches the keychain.
    const QString key = QStringLiteral("test_credential_manager/sentinel");
    const QString sentinel = QStringLiteral("qsettings_sentinel_42");

    QSettings s(QStringLiteral("Remus"), QStringLiteral("Remus"));
    s.setValue(key, sentinel);
    s.sync();

    // Probe keychain status before asserting, so this test is valid in both
    // headless CI (no keychain daemon) and desktop environments.
    const SecretStore::ReadResult kr = SecretStore::readWithStatus(key);
    const QString result = CredentialManager::get(key);

    if (kr.status == SecretStore::ReadResult::Status::BackendError) {
        // Guard must prevent silent plaintext fallback.
        QCOMPARE(result, QString());
    } else if (kr.status == SecretStore::ReadResult::Status::NotFound) {
        // Keychain available and key absent — QSettings fallback is correct.
        QCOMPARE(result, sentinel);
    } else {
        // Key somehow present in keychain; clean state required.
        QSKIP("Key unexpectedly found in keychain; clean keychain state first.");
    }

    s.remove(key);
    s.sync();
}

QTEST_MAIN(CredentialManagerTest)
#include "test_credential_manager.moc"
