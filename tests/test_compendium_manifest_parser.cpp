#include <QtTest/QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QDir>
#include <QTemporaryDir>

#include "../src/core/compendium_manifest_parser.h"

using namespace Remus;

class CompendiumManifestParserTest : public QObject
{
    Q_OBJECT

private slots:
    void readTextFile_success();
    void readTextFile_missingFile();
    void requireString_present();
    void requireString_missingField();
    void requireString_nonStringField();
    void requireString_emptyNotAllowed();
    void requireString_emptyAllowed();
    void parseSourceDescriptor_valid();
    void parseSourceDescriptor_missingSourceId();
    void parseSourceDescriptor_resolvesRelativePathFromManifest();
    void parseSourceDescriptor_resolvesManifestRelativeParentPath();
    void parseManifest_valid();
    void parseManifest_missingBuildId();
    void parseManifest_noSources();
    void parseSourceDescriptor_unsupportedSourceType();
    void parseSourceDescriptor_rejectsJsonSourceType();
    void parseSourceDescriptor_nonStringOptionalField();
    void parseSourceDescriptor_nonIntegerPriority();
};

// ── readTextFile ─────────────────────────────────────────────────────────────

void CompendiumManifestParserTest::readTextFile_success()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath("test.txt");
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write("hello world");
    f.close();

    QString content;
    QString error;
    QVERIFY(readTextFile(path, content, error));
    QCOMPARE(content, QStringLiteral("hello world"));
    QVERIFY(error.isEmpty());
}

void CompendiumManifestParserTest::readTextFile_missingFile()
{
    QString content;
    QString error;
    QVERIFY(!readTextFile(QStringLiteral("/nonexistent/path/file.txt"), content, error));
    QVERIFY(!error.isEmpty());
}

// ── requireString ────────────────────────────────────────────────────────────

void CompendiumManifestParserTest::requireString_present()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("key"), QStringLiteral("value"));

    QString value;
    QString error;
    QVERIFY(requireString(obj, QStringLiteral("key"), value, error));
    QCOMPARE(value, QStringLiteral("value"));
    QVERIFY(error.isEmpty());
}

void CompendiumManifestParserTest::requireString_missingField()
{
    QJsonObject obj;
    QString value;
    QString error;
    QVERIFY(!requireString(obj, QStringLiteral("missing"), value, error));
    QVERIFY(!error.isEmpty());
}

void CompendiumManifestParserTest::requireString_nonStringField()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("num"), 42);

    QString value;
    QString error;
    QVERIFY(!requireString(obj, QStringLiteral("num"), value, error));
    QVERIFY(!error.isEmpty());
}

void CompendiumManifestParserTest::requireString_emptyNotAllowed()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("k"), QStringLiteral(""));

    QString value;
    QString error;
    QVERIFY(!requireString(obj, QStringLiteral("k"), value, error));
    QVERIFY(!error.isEmpty());
}

void CompendiumManifestParserTest::requireString_emptyAllowed()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("k"), QStringLiteral(""));

    QString value;
    QString error;
    QVERIFY(requireString(obj, QStringLiteral("k"), value, error, /*allowEmpty=*/true));
    QVERIFY(value.isEmpty());
}

// ── parseSourceDescriptor ────────────────────────────────────────────────────

void CompendiumManifestParserTest::parseSourceDescriptor_valid()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("source_id"),       QStringLiteral("nointro"));
    obj.insert(QStringLiteral("display_name"),    QStringLiteral("No-Intro"));
    obj.insert(QStringLiteral("source_type"),     QStringLiteral("dat"));
    obj.insert(QStringLiteral("snapshot_id"),     QStringLiteral("snap-001"));
    obj.insert(QStringLiteral("snapshot_label"),  QStringLiteral("Snapshot 1"));
    obj.insert(QStringLiteral("snapshot_ref"),    QStringLiteral("ref-abc"));
    obj.insert(QStringLiteral("path"),            QStringLiteral("/some/path.dat"));
    obj.insert(QStringLiteral("checksum_sha256"), QStringLiteral("abc123"));
    obj.insert(QStringLiteral("priority"),        5);
    obj.insert(QStringLiteral("enabled"),         false);

    CompendiumSourceDescriptor descriptor;
    QString error;
    QVERIFY(parseSourceDescriptor(obj, QStringLiteral("/tmp/manifest.json"), descriptor, error));
    QCOMPARE(descriptor.sourceId,      QStringLiteral("nointro"));
    QCOMPARE(descriptor.displayName,   QStringLiteral("No-Intro"));
    QCOMPARE(descriptor.sourceType,    QStringLiteral("dat"));
    QCOMPARE(descriptor.snapshotId,    QStringLiteral("snap-001"));
    QCOMPARE(descriptor.priority,      5);
    QCOMPARE(descriptor.enabled,       false);
}

void CompendiumManifestParserTest::parseSourceDescriptor_missingSourceId()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("display_name"), QStringLiteral("No-Intro"));

    CompendiumSourceDescriptor descriptor;
    QString error;
    QVERIFY(!parseSourceDescriptor(obj, QStringLiteral("/tmp/manifest.json"), descriptor, error));
    QVERIFY(!error.isEmpty());
}

void CompendiumManifestParserTest::parseSourceDescriptor_resolvesRelativePathFromManifest()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString sourcePath = dir.filePath("source.dat");
    QFile sourceFile(sourcePath);
    QVERIFY(sourceFile.open(QIODevice::WriteOnly | QIODevice::Text));
    sourceFile.write("placeholder");
    sourceFile.close();

    QJsonObject obj;
    obj.insert(QStringLiteral("source_id"), QStringLiteral("nointro"));
    obj.insert(QStringLiteral("display_name"), QStringLiteral("No-Intro"));
    obj.insert(QStringLiteral("source_type"), QStringLiteral("dat"));
    obj.insert(QStringLiteral("snapshot_id"), QStringLiteral("snap-001"));
    obj.insert(QStringLiteral("snapshot_label"), QStringLiteral("Snapshot 1"));
    obj.insert(QStringLiteral("path"), QStringLiteral("source.dat"));
    obj.insert(QStringLiteral("priority"), 5);
    obj.insert(QStringLiteral("enabled"), true);

    CompendiumSourceDescriptor descriptor;
    QString error;
    const QString manifestPath = dir.filePath("manifest.json");
    QVERIFY(parseSourceDescriptor(obj, manifestPath, descriptor, error));
    QCOMPARE(descriptor.path, sourcePath);
}

void CompendiumManifestParserTest::parseSourceDescriptor_resolvesManifestRelativeParentPath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString manifestDir = dir.filePath("data/compendium");
    const QString databasesDir = dir.filePath("data/databases");
    QVERIFY(QDir().mkpath(manifestDir));
    QVERIFY(QDir().mkpath(databasesDir));

    const QString sourcePath = databasesDir + QStringLiteral("/source.dat");
    QFile sourceFile(sourcePath);
    QVERIFY(sourceFile.open(QIODevice::WriteOnly | QIODevice::Text));
    sourceFile.write("placeholder");
    sourceFile.close();

    QJsonObject obj;
    obj.insert(QStringLiteral("source_id"), QStringLiteral("nointro"));
    obj.insert(QStringLiteral("display_name"), QStringLiteral("No-Intro"));
    obj.insert(QStringLiteral("source_type"), QStringLiteral("dat"));
    obj.insert(QStringLiteral("snapshot_id"), QStringLiteral("snap-001"));
    obj.insert(QStringLiteral("snapshot_label"), QStringLiteral("Snapshot 1"));
    obj.insert(QStringLiteral("path"), QStringLiteral("../databases/source.dat"));
    obj.insert(QStringLiteral("priority"), 5);
    obj.insert(QStringLiteral("enabled"), true);

    CompendiumSourceDescriptor descriptor;
    QString error;
    const QString manifestPath = manifestDir + QStringLiteral("/manifest.json");
    QVERIFY(parseSourceDescriptor(obj, manifestPath, descriptor, error));
    QCOMPARE(descriptor.path, sourcePath);
}

// ── parseManifest ─────────────────────────────────────────────────────────────

void CompendiumManifestParserTest::parseManifest_valid()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Build a minimal manifest JSON on disk
    QJsonObject source;
    source.insert(QStringLiteral("source_id"),       QStringLiteral("test-src"));
    source.insert(QStringLiteral("display_name"),    QStringLiteral("Test Source"));
    source.insert(QStringLiteral("source_type"),     QStringLiteral("dat"));
    source.insert(QStringLiteral("snapshot_id"),     QStringLiteral("snap-1"));
    source.insert(QStringLiteral("snapshot_label"),  QStringLiteral("Snap 1"));
    source.insert(QStringLiteral("snapshot_ref"),    QStringLiteral("ref-1"));
    const QString sourcePath = dir.filePath("a.dat");
    QFile sourceFile(sourcePath);
    QVERIFY(sourceFile.open(QIODevice::WriteOnly | QIODevice::Text));
    sourceFile.write("fixture");
    sourceFile.close();

    source.insert(QStringLiteral("path"),            sourcePath);
    source.insert(QStringLiteral("checksum_sha256"), QStringLiteral("deadbeef"));
    source.insert(QStringLiteral("priority"),        10);
    source.insert(QStringLiteral("enabled"),         false);

    QJsonObject manifest;
    manifest.insert(QStringLiteral("build_id"),       QStringLiteral("build-001"));
    manifest.insert(QStringLiteral("schema_version"), 1);
    manifest.insert(QStringLiteral("sources"),        QJsonArray{source});

    const QString manifestPath = dir.filePath("manifest.json");
    QFile f(manifestPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(manifest).toJson());
    f.close();

    QString buildId;
    int schemaVersion = 0;
    QString manifestJson;
    QJsonArray sourceObjects;
    QList<CompendiumSourceDescriptor> sources;
    QString error;

    QVERIFY(parseManifest(manifestPath, buildId, schemaVersion, manifestJson,
                          sourceObjects, sources, error));
    QCOMPARE(buildId,        QStringLiteral("build-001"));
    QCOMPARE(schemaVersion,  1);
    QCOMPARE(sources.size(), 1);
    QCOMPARE(sources.first().sourceId, QStringLiteral("test-src"));
    QVERIFY(!manifestJson.isEmpty());
}

void CompendiumManifestParserTest::parseManifest_missingBuildId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QJsonObject manifest;
    manifest.insert(QStringLiteral("schema_version"), 1);
    manifest.insert(QStringLiteral("sources"), QJsonArray{});

    const QString manifestPath = dir.filePath("manifest.json");
    QFile f(manifestPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(manifest).toJson());
    f.close();

    QString buildId;
    int schemaVersion = 0;
    QString manifestJson;
    QJsonArray sourceObjects;
    QList<CompendiumSourceDescriptor> sources;
    QString error;

    QVERIFY(!parseManifest(manifestPath, buildId, schemaVersion, manifestJson,
                           sourceObjects, sources, error));
    QVERIFY(!error.isEmpty());
}

void CompendiumManifestParserTest::parseManifest_noSources()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QJsonObject manifest;
    manifest.insert(QStringLiteral("build_id"),       QStringLiteral("build-empty"));
    manifest.insert(QStringLiteral("schema_version"), 1);
    manifest.insert(QStringLiteral("sources"),        QJsonArray{});

    const QString manifestPath = dir.filePath("manifest.json");
    QFile f(manifestPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(manifest).toJson());
    f.close();

    QString buildId;
    int schemaVersion = 0;
    QString manifestJson;
    QJsonArray sourceObjects;
    QList<CompendiumSourceDescriptor> sources;
    QString error;

    // Empty sources array is rejected — the parser requires at least one source.
    QVERIFY(!parseManifest(manifestPath, buildId, schemaVersion, manifestJson,
                           sourceObjects, sources, error));
    QVERIFY(!error.isEmpty());
}

void CompendiumManifestParserTest::parseSourceDescriptor_unsupportedSourceType()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("source_id"),      QStringLiteral("bad-src"));
    obj.insert(QStringLiteral("source_type"),    QStringLiteral("csv"));
    obj.insert(QStringLiteral("snapshot_id"),    QStringLiteral("snap-001"));
    obj.insert(QStringLiteral("snapshot_label"), QStringLiteral("Snap 1"));
    obj.insert(QStringLiteral("path"),           QStringLiteral("/fake/path.csv"));
    obj.insert(QStringLiteral("priority"),       5);
    obj.insert(QStringLiteral("enabled"),        false);

    CompendiumSourceDescriptor descriptor;
    QString error;
    QVERIFY(!parseSourceDescriptor(obj, QStringLiteral("/tmp/manifest.json"), descriptor, error));
    QVERIFY(!error.isEmpty());
    QVERIFY2(error.contains(QStringLiteral("csv")), qPrintable(error));
}

void CompendiumManifestParserTest::parseSourceDescriptor_rejectsJsonSourceType()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("source_id"),      QStringLiteral("json-src"));
    obj.insert(QStringLiteral("source_type"),    QStringLiteral("json"));
    obj.insert(QStringLiteral("snapshot_id"),    QStringLiteral("snap-001"));
    obj.insert(QStringLiteral("snapshot_label"), QStringLiteral("Snap 1"));
    obj.insert(QStringLiteral("path"),           QStringLiteral("/fake/path.json"));
    obj.insert(QStringLiteral("priority"),       5);
    obj.insert(QStringLiteral("enabled"),        false);

    CompendiumSourceDescriptor descriptor;
    QString error;
    QVERIFY(!parseSourceDescriptor(obj, QStringLiteral("/tmp/manifest.json"), descriptor, error));
    QVERIFY2(error.contains(QStringLiteral("expected: dat")), qPrintable(error));
}

void CompendiumManifestParserTest::parseSourceDescriptor_nonStringOptionalField()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("source_id"),      QStringLiteral("test-src"));
    obj.insert(QStringLiteral("source_type"),    QStringLiteral("dat"));
    obj.insert(QStringLiteral("snapshot_id"),    QStringLiteral("snap-001"));
    obj.insert(QStringLiteral("snapshot_label"), QStringLiteral("Snap 1"));
    obj.insert(QStringLiteral("path"),           QStringLiteral("/fake/path.dat"));
    obj.insert(QStringLiteral("license_id"),     42);  // must be a string, not a number
    obj.insert(QStringLiteral("priority"),       5);
    obj.insert(QStringLiteral("enabled"),        false);

    CompendiumSourceDescriptor descriptor;
    QString error;
    QVERIFY(!parseSourceDescriptor(obj, QStringLiteral("/tmp/manifest.json"), descriptor, error));
    QVERIFY(!error.isEmpty());
}

void CompendiumManifestParserTest::parseSourceDescriptor_nonIntegerPriority()
{
    QJsonObject obj;
    obj.insert(QStringLiteral("source_id"),      QStringLiteral("test-src"));
    obj.insert(QStringLiteral("source_type"),    QStringLiteral("dat"));
    obj.insert(QStringLiteral("snapshot_id"),    QStringLiteral("snap-001"));
    obj.insert(QStringLiteral("snapshot_label"), QStringLiteral("Snap 1"));
    obj.insert(QStringLiteral("path"),           QStringLiteral("/fake/path.dat"));
    obj.insert(QStringLiteral("priority"),       10.5);  // must be an integer
    obj.insert(QStringLiteral("enabled"),        false);

    CompendiumSourceDescriptor descriptor;
    QString error;
    QVERIFY(!parseSourceDescriptor(obj, QStringLiteral("/tmp/manifest.json"), descriptor, error));
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(CompendiumManifestParserTest)
#include "test_compendium_manifest_parser.moc"
