#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QSignalSpy>
#include "core/scanner.h"

using namespace Remus;

class ScannerTest : public QObject {
    Q_OBJECT

private slots:
    void missingDirectoryEmitsError();
    void cancelStopsScan();
    void multiFileLinking();
};

static QString writeFile(const QString &path, const QByteArray &data = QByteArray("data"))
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        return QString();
    }
    f.write(data);
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
    QTextStream out(&gdi);
    out << "2" << '\n';
    out << "1 0 4 2352 track01.bin" << '\n';
    out << "2 0 4 2352 track02.bin" << '\n';
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

QTEST_MAIN(ScannerTest)
#include "test_scanner.moc"
