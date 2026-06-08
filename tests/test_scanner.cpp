#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
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
    void cancelDuringArchivePhaseReturnsPartialResults();
    void archiveFilePathScansArchiveContents();
    void multiFileLinking();
    void compressedArchiveMultiFileLinking();
    void markdownDocumentsAreSkippedButGenesisRomFilesRemain();
    void exclusionMarkerChangesAreRespectedAcrossScans();
    void concurrentScanNoDuplicates();
};

static QString writeFile(const QString &path, const QByteArray &data = QByteArray("data")) {
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

static QString findSevenZip() {
    const QStringList candidates = { "7z", "7za", "7zz" };
    for (const QString &candidate : candidates) {
        const QString executable = QStandardPaths::findExecutable(candidate);
        if (!executable.isEmpty()) {
            return executable;
        }
    }

    return QString();
}

static bool createArchive(
    const QString &program, const QString &workingDirectory, const QString &archivePath, const QStringList &inputs) {
    QProcess process;
    process.setWorkingDirectory(workingDirectory);
    process.start(program, QStringList { "a", "-t7z", archivePath } + inputs);
    if (!process.waitForStarted()) {
        return false;
    }
    if (!process.waitForFinished(30000)) {
        return false;
    }

    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

void ScannerTest::missingDirectoryEmitsError() {
    Scanner scanner;
    scanner.setArchiveScanning(false);
    QSignalSpy errSpy(&scanner, &Scanner::scanError);
    QList<ScanResult> results = scanner.scan("/path/does/not/exist");
    QCOMPARE(results.size(), 0);
    QVERIFY(!errSpy.isEmpty());
}

void ScannerTest::cancelStopsScan() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // Create a bunch of files so we can cancel mid-scan
    const int fileCount = 50;
    for (int i = 0; i < fileCount; ++i) {
        QVERIFY(!writeFile(dir.filePath(QString("file_%1.nes").arg(i))).isEmpty());
    }

    Scanner scanner;
    scanner.setExtensions({ ".nes" });
    scanner.setArchiveScanning(false);

    connect(&scanner, &Scanner::fileFound, &scanner, [&scanner]() { scanner.requestCancel(); });

    QList<ScanResult> results = scanner.scan(dir.path());
    QVERIFY(scanner.wasCancelled());
    QVERIFY(results.size() < fileCount); // Should stop early
}

void ScannerTest::archiveFilePathScansArchiveContents() {
    const QString sevenZip = findSevenZip();
    if (sevenZip.isEmpty()) {
        QSKIP("7z not available");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());

    QVERIFY(!writeFile(sourceDir.filePath("game.iso"), QByteArray(1024, '\0')).isEmpty());

    const QString archivePath = dir.filePath("game.7z");
    QVERIFY(createArchive(sevenZip, sourceDir.path(), archivePath, { "game.iso" }));

    Scanner scanner;
    scanner.setExtensions({ ".iso" });

    const QList<ScanResult> results = scanner.scan(archivePath);
    QCOMPARE(results.size(), 1);
    QVERIFY(results.first().isCompressed);
    QCOMPARE(results.first().archivePath, archivePath);
    QCOMPARE(results.first().archiveInternalPath, QStringLiteral("game.iso"));
}

void ScannerTest::multiFileLinking() {
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
    scanner.setExtensions({ ".cue", ".bin", ".gdi" });
    scanner.setArchiveScanning(false);
    QList<ScanResult> results = scanner.scan(dir.path());

    // Expect cue + 3 bins + gdi = 5 entries
    QCOMPARE(results.size(), 5);

    // Find cue linked to bin (data track is primary, sheet is secondary)
    auto itCue = std::find_if(results.begin(), results.end(),
        [](const ScanResult &r) { return r.extension == ".cue" && r.parentFilePath.endsWith("game.bin"); });
    QVERIFY(itCue != results.end());
    QCOMPARE(itCue->isPrimary, false);

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

void ScannerTest::compressedArchiveMultiFileLinking() {
    const QString sevenZip = findSevenZip();
    if (sevenZip.isEmpty()) {
        QSKIP("7z not available");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());

    QVERIFY(QDir().mkpath(sourceDir.filePath("cue")));
    QVERIFY(QDir().mkpath(sourceDir.filePath("gdi")));

    const QByteArray cueContents = QByteArrayLiteral("FILE \"game.bin\" BINARY\n"
                                                     "  TRACK 01 MODE1/2352\n"
                                                     "    INDEX 01 00:00:00\n");
    QVERIFY(!writeFile(sourceDir.filePath("cue/game.cue"), cueContents).isEmpty());
    QVERIFY(!writeFile(sourceDir.filePath("cue/game.bin")).isEmpty());

    const QByteArray gdiContents = QByteArrayLiteral("3\n"
                                                     "1 0 4 2352 \"track01.bin\" 0\n"
                                                     "2 45000 0 2352 \"track02.raw\" 0\n"
                                                     "3 90000 4 2352 \"track03.bin\" 0\n");
    QVERIFY(!writeFile(sourceDir.filePath("gdi/disc.gdi"), gdiContents).isEmpty());
    QVERIFY(!writeFile(sourceDir.filePath("gdi/track01.bin")).isEmpty());
    QVERIFY(!writeFile(sourceDir.filePath("gdi/track02.raw")).isEmpty());
    QVERIFY(!writeFile(sourceDir.filePath("gdi/track03.bin")).isEmpty());

    const QString cueArchive = dir.filePath("cue_set.7z");
    const QString gdiArchive = dir.filePath("gdi_set.7z");
    QVERIFY(createArchive(sevenZip, sourceDir.path(), cueArchive, { "cue" }));
    QVERIFY(createArchive(sevenZip, sourceDir.path(), gdiArchive, { "gdi" }));

    Scanner scanner;
    scanner.setExtensions({ ".cue", ".bin", ".raw", ".gdi" });
    QList<ScanResult> results = scanner.scan(dir.path());

    QCOMPARE(results.size(), 6);

    auto findEntry = [&](const QString &archiveName, const QString &memberSuffix) -> ScanResult * {
        for (auto &result : results) {
            if (result.archivePath.endsWith(archiveName) && result.archiveInternalPath.endsWith(memberSuffix)) {
                return &result;
            }
        }
        return nullptr;
    };

    ScanResult *cueFile = findEntry("cue_set.7z", "cue/game.cue");
    ScanResult *binFile = findEntry("cue_set.7z", "cue/game.bin");
    QVERIFY(cueFile != nullptr);
    QVERIFY(binFile != nullptr);
    QVERIFY(!cueFile->isPrimary);
    QVERIFY(binFile->isPrimary);
    QVERIFY(cueFile->parentFilePath.endsWith("cue_set.7z::cue/game.bin"));

    ScanResult *gdiFile = findEntry("gdi_set.7z", "gdi/disc.gdi");
    QVERIFY(gdiFile != nullptr);
    QVERIFY(gdiFile->isPrimary);

    const QStringList trackMembers = { "gdi/track01.bin", "gdi/track02.raw", "gdi/track03.bin" };
    for (const QString &member : trackMembers) {
        ScanResult *trackFile = findEntry("gdi_set.7z", member);
        QVERIFY(trackFile != nullptr);
        QVERIFY(!trackFile->isPrimary);
        QVERIFY(trackFile->parentFilePath.endsWith("gdi_set.7z::gdi/disc.gdi"));
    }
}

void ScannerTest::markdownDocumentsAreSkippedButGenesisRomFilesRemain() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString readmePath = dir.filePath("README.md");
    QVERIFY(!writeFile(readmePath, QByteArray("# ROM folder\nThis is documentation.\n")).isEmpty());

    const QString romPath = dir.filePath("Sonic The Hedgehog (USA, Europe).md");
    const QByteArray romData = QByteArray::fromHex("00010203FF80AA55");
    QVERIFY(!writeFile(romPath, romData).isEmpty());

    Scanner scanner;
    scanner.setExtensions({ ".md" });
    scanner.setArchiveScanning(false);

    QList<ScanResult> results = scanner.scan(dir.path());
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().filename, QString("Sonic The Hedgehog (USA, Europe).md"));
}

void ScannerTest::exclusionMarkerChangesAreRespectedAcrossScans() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString romPath = dir.filePath("game.nes");
    QVERIFY(!writeFile(romPath).isEmpty());

    Scanner scanner;
    scanner.setExtensions({ ".nes" });
    scanner.setArchiveScanning(false);

    QList<ScanResult> initialResults = scanner.scan(dir.path());
    QCOMPARE(initialResults.size(), 1);

    const QString markerPath = dir.filePath(Constants::Settings::Files::MARKER_SKIP_SCAN);
    QVERIFY(!writeFile(markerPath, QByteArray("skip")).isEmpty());

    QList<ScanResult> excludedResults = scanner.scan(dir.path());
    QCOMPARE(excludedResults.size(), 0);
}

void ScannerTest::cancelDuringArchivePhaseReturnsPartialResults() {
    // Finding #2 — cancellation should stop archive member iteration mid-archive,
    // not only between archive files. After requestCancel() fires on the first
    // fileFound signal, subsequent members of the same archive must not be returned.
    const QString sevenZip = findSevenZip();
    if (sevenZip.isEmpty()) {
        QSKIP("7z not available");
    }

    QTemporaryDir dir;
    QTemporaryDir sourceDir;
    QVERIFY(dir.isValid() && sourceDir.isValid());

    // One archive containing many ROM entries so cancellation has entries to skip.
    const int entryCount = 50;
    QStringList inputs;
    for (int i = 0; i < entryCount; ++i) {
        QVERIFY(!writeFile(sourceDir.filePath(QString("rom_%1.nes").arg(i))).isEmpty());
        inputs << QString("rom_%1.nes").arg(i);
    }
    const QString archivePath = dir.filePath("big.7z");
    QVERIFY(createArchive(sevenZip, sourceDir.path(), archivePath, inputs));

    Scanner scanner;
    scanner.setExtensions({ ".nes" });

    // Cancel on the very first discovered entry; the loop check inside
    // processArchiveWithExtractor should then break before appending the rest.
    connect(&scanner, &Scanner::fileFound, &scanner, [&scanner]() { scanner.requestCancel(); }, Qt::DirectConnection);

    const QList<ScanResult> results = scanner.scan(dir.path());
    QVERIFY(scanner.wasCancelled());
    QVERIFY(results.size() < entryCount);
}

void ScannerTest::concurrentScanNoDuplicates() {
    const QString sevenZip = findSevenZip();
    if (sevenZip.isEmpty()) {
        QSKIP("7z not available");
    }

    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QTemporaryDir sourceDir;
    QVERIFY(sourceDir.isValid());

    // Plain ROM files — processed sequentially in Phase 1
    const int plainFileCount = 10;
    for (int i = 0; i < plainFileCount; ++i) {
        QVERIFY(!writeFile(dir.filePath(QString("plain_%1.nes").arg(i))).isEmpty());
    }

    // Archives each containing two ROM files — processed concurrently in Phase 2
    const int archiveCount = 4;
    const int filesPerArchive = 2;
    for (int a = 0; a < archiveCount; ++a) {
        const QString srcSubDir = QString("arc_%1").arg(a);
        QVERIFY(QDir().mkpath(sourceDir.filePath(srcSubDir)));
        for (int f = 0; f < filesPerArchive; ++f) {
            QVERIFY(!writeFile(sourceDir.filePath(srcSubDir + QString("/rom_%1_%2.nes").arg(a).arg(f))).isEmpty());
        }
        const QString archivePath = dir.filePath(QString("archive_%1.7z").arg(a));
        QVERIFY(createArchive(sevenZip, sourceDir.path(), archivePath, { srcSubDir }));
    }

    Scanner scanner;
    scanner.setExtensions({ ".nes" });

    const QList<ScanResult> results = scanner.scan(dir.path());

    const int expectedCount = plainFileCount + archiveCount * filesPerArchive;
    QCOMPARE(results.size(), expectedCount);

    // No duplicate entries — each path+internal combination must appear exactly once
    QSet<QString> seen;
    for (const ScanResult &r : results) {
        const QString key = r.isCompressed ? r.archivePath + QLatin1String("::") + r.archiveInternalPath : r.path;
        QVERIFY2(!seen.contains(key), qPrintable(QLatin1String("Duplicate result: ") + key));
        seen.insert(key);
    }
}

QTEST_MAIN(ScannerTest)
#include "test_scanner.moc"
