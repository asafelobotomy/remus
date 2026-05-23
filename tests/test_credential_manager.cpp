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

#include "../src/services/credential_manager.h"
#include "../src/services/secret_store.h"

using namespace Remus;

class CredentialManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void testJsonTakesPriorityOverEnvVar();
    void testNonexistentJsonFileFallsThrough();
    void testEnvVarResolvesForMappedKey();
    void testEmptyReturnedWhenNothingSet();
    void testBackendErrorDoesNotFallThroughToQSettings();
};

// ── Helpers ────────────────────────────────────────────────────────────────

/// Write a minimal credentials JSON file with one outer/inner key.
static QString writeCredJson(const QTemporaryDir &dir,
                             const QString &outer,
                             const QString &inner,
                             const QString &value)
{
    const QJsonObject inner_obj{{inner, value}};
    const QJsonObject root{{outer, inner_obj}};
    const QJsonDocument doc(root);
    const QString path = dir.filePath(QStringLiteral("creds.json"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    f.write(doc.toJson());
    return path;
}

// ── Tests ──────────────────────────────────────────────────────────────────

/// JSON file value must beat a simultaneously-set env var (step 1 > step 2).
void CredentialManagerTest::testJsonTakesPriorityOverEnvVar()
{
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
void CredentialManagerTest::testNonexistentJsonFileFallsThrough()
{
    const QString missingPath = QStringLiteral("/nonexistent/path/creds.json");
    qputenv("REMUS_IGDB_CLIENT_SECRET", "env_secret");
    const QString result =
        CredentialManager::get("igdb/client_secret", missingPath);
    qunsetenv("REMUS_IGDB_CLIENT_SECRET");

    QCOMPARE(result, QStringLiteral("env_secret"));
}

/// An env var in the built-in mapping table must be returned as-is (step 2).
void CredentialManagerTest::testEnvVarResolvesForMappedKey()
{
    qputenv("REMUS_TGDB_API_KEY", "tgdb_abc123");
    const QString result = CredentialManager::get("thegamesdb/api_key");
    qunsetenv("REMUS_TGDB_API_KEY");

    QCOMPARE(result, QStringLiteral("tgdb_abc123"));
}

/// A key absent from every source must produce an empty string, not a crash.
void CredentialManagerTest::testEmptyReturnedWhenNothingSet()
{
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
void CredentialManagerTest::testBackendErrorDoesNotFallThroughToQSettings()
{
    // Use a key with no env-var mapping so the lookup reaches the keychain.
    const QString key  = QStringLiteral("test_credential_manager/sentinel");
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
