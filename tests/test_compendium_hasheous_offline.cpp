#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "../src/metadata/compendium_hasheous_offline.h"

using namespace Remus::Compendium;

class CompendiumHasheousOfflineTest : public QObject {
    Q_OBJECT

private:
    QString fixturePath(const QString &name) const {
        const QStringList candidates = {
            QString(REMUS_SOURCE_DIR) + "/tests/fixtures/" + name,
            QDir::currentPath() + "/tests/fixtures/" + name,
            QCoreApplication::applicationDirPath() + "/../../tests/fixtures/" + name,
        };
        for (const QString &path : candidates) {
            if (QFile::exists(path))
                return QDir::cleanPath(path);
        }
        return { };
    }

private slots:
    void loadHasheousOfflineIndex_buildsLookupFromFixtureDump();
    void lookupHasheousOfflineMatch_findsByAnyHashType();
    void hasHasheousOfflineDumpFiles_detectsJsonUnderRoot();
};

void CompendiumHasheousOfflineTest::loadHasheousOfflineIndex_buildsLookupFromFixtureDump() {
    const QString fixtureFile = fixturePath(QStringLiteral("hasheous_offline/sample_entry.json"));
    QVERIFY2(!fixtureFile.isEmpty(), "Hasheous offline fixture not found");

    QTemporaryDir dumpDir;
    QVERIFY(dumpDir.isValid());

    const QString destPath = dumpDir.filePath(QStringLiteral("NintendoDS/sample_entry.json"));
    QVERIFY(QDir().mkpath(QFileInfo(destPath).absolutePath()));
    QVERIFY(QFile::copy(fixtureFile, destPath));

    QHash<QString, HasheousOfflineMatch> index;
    QString error;
    QVERIFY2(loadHasheousOfflineIndex(dumpDir.path(), index, error), qPrintable(error));
    QVERIFY(!index.isEmpty());

    HasheousOfflineMatch match;
    QVERIFY(lookupHasheousOfflineMatch(index, QStringLiteral("deadbeef"), QString(), QString(), QString(), match));
    QCOMPARE(match.igdbId, QStringLiteral("424242"));
    QCOMPARE(match.description, QStringLiteral("Fixture game for offline Hasheous index tests"));
    QCOMPARE(match.genre, QStringLiteral("Platform"));
    QCOMPARE(match.raGameId, QStringLiteral("9999"));
}

void CompendiumHasheousOfflineTest::lookupHasheousOfflineMatch_findsByAnyHashType() {
    QHash<QString, HasheousOfflineMatch> index;
    HasheousOfflineMatch expected;
    expected.igdbId = QStringLiteral("1");
    index.insert(QStringLiteral("md5:0123456789abcdef0123456789abcdef"), expected);

    HasheousOfflineMatch out;
    QVERIFY(lookupHasheousOfflineMatch(
        index, QString(), QStringLiteral("0123456789abcdef0123456789abcdef"), QString(), QString(), out));
    QCOMPARE(out.igdbId, QStringLiteral("1"));
}

void CompendiumHasheousOfflineTest::hasHasheousOfflineDumpFiles_detectsJsonUnderRoot() {
    QTemporaryDir emptyDir;
    QVERIFY(emptyDir.isValid());
    QVERIFY(!hasHasheousOfflineDumpFiles(emptyDir.path()));

    QTemporaryDir dumpDir;
    QVERIFY(dumpDir.isValid());
    QFile json(dumpDir.filePath(QStringLiteral("sample.json")));
    QVERIFY(json.open(QIODevice::WriteOnly));
    json.write("{}");
    json.close();
    QVERIFY(hasHasheousOfflineDumpFiles(dumpDir.path()));
}

QTEST_MAIN(CompendiumHasheousOfflineTest)
#include "test_compendium_hasheous_offline.moc"
