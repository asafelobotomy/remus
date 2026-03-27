#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QSignalSpy>
#include "core/scanner.h"
#include "core/constants/settings.h"

using namespace Remus;

class ScannerTest : public QObject {
    Q_OBJECT

private slots:
    void missingDirectoryEmitsError();
    void cancelStopsScan();
    void multiFileLinking();
    void markdownDocumentsAreSkippedButGenesisRomFilesRemain();
    void exclusionMarkerChangesAreRespectedAcrossScans();
};

static QString writeFile(const QString &path, const QByteArray &data = QByteArray("data"))
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return QString();
    }
    if (f.write(data) != data.size()) {
        return QString();
    }
    f.close();
    return path;
}

void ScannerTest::missingDirectoryEmitsError()
{
    Scanner scanner;
    scanner.setArchiveScanning(false);
    QSignalSpy errSpy(&scanner, &Scanner::scanError);
    QList<ScanResult> results = scanner.scan("/path/does/not/exist");
    QCOMPARE(results.size(), 0);
    QVERIFY(!errSpy.isEmpty());
}

void ScannerTest::cancelStopsScan()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // Create a bunch of files so we can cancel mid-scan
    const int fileCount = 50;
    for (int i = 0; i < fileCount; ++i) {
        QVERIFY(!writeFile(dir.filePath(QString("file_%1.nes").arg(i))).isEmpty());
    }

    Scanner scanner;
    scanner.setExtensions({".nes"});
    scanner.setArchiveScanning(false);

    connect(&scanner, &Scanner::fileFound, &scanner, [&scanner]() {
        scanner.requestCancel();
    });

    QList<ScanResult> results = scanner.scan(dir.path());
    QVERIFY(scanner.wasCancelled());
    QVERIFY(results.size() < fileCount);  // Should stop early
}

void ScannerTest::multiFileLinking()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString cuePath = writeFile(dir.filePath("game.cue"));
    const QString binPath = writeFile(dir.filePath("game.bin"));
    QVERIFY(!cuePath.isEmpty() && !binPath.isEmpty());

    const QString gdiPath = dir.filePath("disc.gdi");
    QFile gdi(gdiPath);
    QVERIFY(gdi.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray gdiContents = QByteArrayLiteral("2\n1 0 4 2352 track01.bin\n2 0 4 2352 track02.bin\n");
    QVERIFY(gdi.write(gdiContents) == gdiContents.size());
    gdi.close();
    QVERIFY(!writeFile(dir.filePath("track01.bin")).isEmpty());
    QVERIFY(!writeFile(dir.filePath("track02.bin")).isEmpty());

    Scanner scanner;
    scanner.setExtensions({".cue", ".bin", ".gdi"});
    scanner.setArchiveScanning(false);
    QList<ScanResult> results = scanner.scan(dir.path());

    // Expect cue + 3 bins + gdi = 5 entries
    QCOMPARE(results.size(), 5);

    // Find bin linked to cue
    auto itBin = std::find_if(results.begin(), results.end(), [](const ScanResult &r) {
        return r.extension == ".bin" && r.parentFilePath.endsWith("game.cue");
    });
    QVERIFY(itBin != results.end());
    QCOMPARE(itBin->isPrimary, false);

    // GDI tracks linked to gdi parent
    int linkedTracks = 0;
    for (const auto &res : results) {
        if (res.path.endsWith("track01.bin") || res.path.endsWith("track02.bin")) {
            QVERIFY(!res.isPrimary);
            QVERIFY(res.parentFilePath.endsWith("disc.gdi"));
            linkedTracks++;
        }
    }
    QCOMPARE(linkedTracks, 2);
}

void ScannerTest::markdownDocumentsAreSkippedButGenesisRomFilesRemain()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString readmePath = dir.filePath("README.md");
    QVERIFY(!writeFile(readmePath, QByteArray("# ROM folder\nThis is documentation.\n")).isEmpty());

    const QString romPath = dir.filePath("Sonic The Hedgehog (USA, Europe).md");
    const QByteArray romData = QByteArray::fromHex("00010203FF80AA55");
    QVERIFY(!writeFile(romPath, romData).isEmpty());

    Scanner scanner;
    scanner.setExtensions({".md"});
    scanner.setArchiveScanning(false);

    QList<ScanResult> results = scanner.scan(dir.path());
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().filename, QString("Sonic The Hedgehog (USA, Europe).md"));
}

void ScannerTest::exclusionMarkerChangesAreRespectedAcrossScans()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString romPath = dir.filePath("game.nes");
    QVERIFY(!writeFile(romPath).isEmpty());

    Scanner scanner;
    scanner.setExtensions({".nes"});
    scanner.setArchiveScanning(false);

    QList<ScanResult> initialResults = scanner.scan(dir.path());
    QCOMPARE(initialResults.size(), 1);

    const QString markerPath = dir.filePath(Constants::Settings::Files::MARKER_SKIP_SCAN);
    QVERIFY(!writeFile(markerPath, QByteArray("skip")).isEmpty());

    QList<ScanResult> excludedResults = scanner.scan(dir.path());
    QCOMPARE(excludedResults.size(), 0);
}

QTEST_MAIN(ScannerTest)
#include "test_scanner.moc"
