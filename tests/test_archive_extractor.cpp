#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <archive.h>
#include <archive_entry.h>

#include "archive_extractor.h"

using namespace Remus;

// ─────────────────────────────────────────────────────────────────────────────
// Test archive helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool createTestArchive(
    const QString &path, const QMap<QString, QByteArray> &files, ArchiveFormat fmt = ArchiveFormat::ZIP) {
    struct archive *a = archive_write_new();
    if (fmt == ArchiveFormat::SevenZip)
        archive_write_set_format_7zip(a);
    else
        archive_write_set_format_zip(a);

    if (archive_write_open_filename(a, path.toUtf8().constData()) != ARCHIVE_OK) {
        archive_write_free(a);
        return false;
    }

    for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, it.key().toUtf8().constData());
        archive_entry_set_size(entry, it.value().size());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(a, entry);
        archive_write_data(a, it.value().constData(), static_cast<size_t>(it.value().size()));
        archive_entry_free(entry);
    }

    archive_write_close(a);
    archive_write_free(a);
    return QFileInfo::exists(path);
}

static bool createArchiveWithUnsafePath(const QString &path) {
    struct archive *a = archive_write_new();
    archive_write_set_format_zip(a);
    if (archive_write_open_filename(a, path.toUtf8().constData()) != ARCHIVE_OK) {
        archive_write_free(a);
        return false;
    }

    struct archive_entry *entry = archive_entry_new();
    archive_entry_set_pathname(entry, "../evil.bin");
    archive_entry_set_size(entry, 4);
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_write_header(a, entry);
    archive_write_data(a, "evil", 4);
    archive_entry_free(entry);

    archive_write_close(a);
    archive_write_free(a);
    return QFileInfo::exists(path);
}

static bool createArchiveWithOrderedEntries(const QString &path, const QList<QPair<QString, QByteArray>> &files) {
    struct archive *a = archive_write_new();
    archive_write_set_format_zip(a);
    if (archive_write_open_filename(a, path.toUtf8().constData()) != ARCHIVE_OK) {
        archive_write_free(a);
        return false;
    }

    for (const auto &file : files) {
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, file.first.toUtf8().constData());
        archive_entry_set_size(entry, file.second.size());
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(a, entry);
        archive_write_data(a, file.second.constData(), static_cast<size_t>(file.second.size()));
        archive_entry_free(entry);
    }

    archive_write_close(a);
    archive_write_free(a);
    return QFileInfo::exists(path);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test class
// ─────────────────────────────────────────────────────────────────────────────

class ArchiveExtractorTest : public QObject {
    Q_OBJECT

private slots:
    void testDetectFormat();
    void testNormalizeArchiveMemberPath();
    void testCanExtractAllSupportedFormats();
    void testGetArchiveInfoZip();
    void testGetArchiveInfo7z();
    void testExtractZip();
    void testExtract7zCreatesSubfolder();
    void testExtractFilePreservesNestedRelativePath();
    void testExtractRejectsUnsafeArchiveEntries();
    void testExtractRejectsMixedUnsafeArchiveWithoutWritingFiles();
    void testExtractContinuesBelowFailureThreshold();
    void testExtractFailsAtOneToThreeFailureRatio();
    void testBatchExtractCanBeCancelledAfterFirstItem();
    void testExtractUnsupported();
    void testReadMemberPrefix();
};

// ─────────────────────────────────────────────────────────────────────────────
// Tests
// ─────────────────────────────────────────────────────────────────────────────

void ArchiveExtractorTest::testDetectFormat() {
    QCOMPARE(ArchiveExtractor::detectFormat("game.zip"), ArchiveFormat::ZIP);
    QCOMPARE(ArchiveExtractor::detectFormat("game.7z"), ArchiveFormat::SevenZip);
    QCOMPARE(ArchiveExtractor::detectFormat("game.rar"), ArchiveFormat::RAR);
    QCOMPARE(ArchiveExtractor::detectFormat("game.tgz"), ArchiveFormat::TarGz);
    QCOMPARE(ArchiveExtractor::detectFormat("game.gz"), ArchiveFormat::GZip);
    QCOMPARE(ArchiveExtractor::detectFormat("game.bz2"), ArchiveFormat::TarBz2);
    QCOMPARE(ArchiveExtractor::detectFormat("game.tbz2"), ArchiveFormat::TarBz2);
    QCOMPARE(ArchiveExtractor::detectFormat("game.tar"), ArchiveFormat::Tar);
    QCOMPARE(ArchiveExtractor::detectFormat("game.xz"), ArchiveFormat::TarXz);
    QCOMPARE(ArchiveExtractor::detectFormat("game.bin"), ArchiveFormat::Unknown);
    QCOMPARE(ArchiveExtractor::detectFormat("game"), ArchiveFormat::Unknown);
}

void ArchiveExtractorTest::testNormalizeArchiveMemberPath() {
    QCOMPARE(ArchiveExtractor::normalizeArchiveMemberPath("game.bin"), QStringLiteral("game.bin"));
    QCOMPARE(ArchiveExtractor::normalizeArchiveMemberPath("subdir/game.bin"), QStringLiteral("subdir/game.bin"));
    QCOMPARE(ArchiveExtractor::normalizeArchiveMemberPath("./subdir/game.bin"), QStringLiteral("subdir/game.bin"));
    QCOMPARE(ArchiveExtractor::normalizeArchiveMemberPath("../evil"), QString());
    QCOMPARE(ArchiveExtractor::normalizeArchiveMemberPath("/absolute"), QString());
    QCOMPARE(ArchiveExtractor::normalizeArchiveMemberPath(""), QString());
    QCOMPARE(ArchiveExtractor::normalizeArchiveMemberPath("."), QString());
    QCOMPARE(ArchiveExtractor::normalizeArchiveMemberPath("a/../../b"), QString());
    QCOMPARE(ArchiveExtractor::normalizeArchiveMemberPath("back\\slash"), QStringLiteral("back/slash"));
}

void ArchiveExtractorTest::testCanExtractAllSupportedFormats() {
    ArchiveExtractor extractor;
    QVERIFY(extractor.canExtract(ArchiveFormat::ZIP));
    QVERIFY(extractor.canExtract(ArchiveFormat::SevenZip));
    QVERIFY(extractor.canExtract(ArchiveFormat::RAR));
    QVERIFY(extractor.canExtract(ArchiveFormat::GZip));
    QVERIFY(extractor.canExtract(ArchiveFormat::TarGz));
    QVERIFY(extractor.canExtract(ArchiveFormat::TarBz2));
    QVERIFY(extractor.canExtract(ArchiveFormat::Tar));
    QVERIFY(extractor.canExtract(ArchiveFormat::TarXz));
    QVERIFY(!extractor.canExtract(ArchiveFormat::Unknown));
}

void ArchiveExtractorTest::testGetArchiveInfoZip() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString archivePath = tmp.filePath("test.zip");
    QMap<QString, QByteArray> files;
    files["game.bin"] = QByteArray(256, 'A');
    files["subdir/readme.txt"] = QByteArray("hello");
    QVERIFY(createTestArchive(archivePath, files, ArchiveFormat::ZIP));

    ArchiveExtractor extractor;
    const ArchiveInfo info = extractor.getArchiveInfo(archivePath);

    QCOMPARE(info.fileCount, 2);
    QVERIFY(info.contents.contains(QStringLiteral("game.bin")));
    QVERIFY(info.contents.contains(QStringLiteral("subdir/readme.txt")));
    QCOMPARE(info.uncompressedSize, static_cast<qint64>(256 + 5));
    QVERIFY(info.unsafeEntries.isEmpty());
}

void ArchiveExtractorTest::testGetArchiveInfo7z() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString archivePath = tmp.filePath("test.7z");
    QMap<QString, QByteArray> files;
    files["disc.iso"] = QByteArray(512, 'B');
    QVERIFY(createTestArchive(archivePath, files, ArchiveFormat::SevenZip));

    ArchiveExtractor extractor;
    const ArchiveInfo info = extractor.getArchiveInfo(archivePath);

    QCOMPARE(info.fileCount, 1);
    QVERIFY(info.contents.contains(QStringLiteral("disc.iso")));
    QCOMPARE(info.uncompressedSize, static_cast<qint64>(512));
}

void ArchiveExtractorTest::testExtractZip() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString archivePath = tmp.filePath("game.zip");
    const QByteArray payload(128, 'X');
    QMap<QString, QByteArray> files;
    files["game.bin"] = payload;
    QVERIFY(createTestArchive(archivePath, files, ArchiveFormat::ZIP));

    ArchiveExtractor extractor;
    const QString outDir = tmp.filePath("out");
    const ExtractionResult result = extractor.extract(archivePath, outDir, false);

    QVERIFY(result.success);
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.filesExtracted, 1);
    QVERIFY(QFileInfo::exists(outDir + "/game.bin"));
    QCOMPARE(QFile(outDir + "/game.bin").size(), static_cast<qint64>(128));
}

void ArchiveExtractorTest::testExtract7zCreatesSubfolder() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString archivePath = tmp.filePath("mygame.7z");
    QMap<QString, QByteArray> files;
    files["disc.iso"] = QByteArray(64, 'Y');
    QVERIFY(createTestArchive(archivePath, files, ArchiveFormat::SevenZip));

    ArchiveExtractor extractor;
    const QString baseOut = tmp.filePath("roms");
    const ExtractionResult result = extractor.extract(archivePath, baseOut, /*createSubfolder=*/true);

    QVERIFY(result.success);
    // File should be inside <baseOut>/mygame/
    const QString expectedPath = baseOut + "/mygame/disc.iso";
    QVERIFY(QFileInfo::exists(expectedPath));
}

void ArchiveExtractorTest::testExtractFilePreservesNestedRelativePath() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString archivePath = tmp.filePath("multi.zip");
    QMap<QString, QByteArray> files;
    files["a.bin"] = QByteArray("aaa");
    files["sub/b.bin"] = QByteArray("bbb");
    QVERIFY(createTestArchive(archivePath, files, ArchiveFormat::ZIP));

    ArchiveExtractor extractor;
    const QString outDir = tmp.filePath("single_out");
    const ExtractionResult result = extractor.extractFile(archivePath, "sub/b.bin", outDir);

    QVERIFY(result.success);
    QCOMPARE(result.filesExtracted, 1);
    QVERIFY(QFileInfo::exists(outDir + "/sub/b.bin"));
}

void ArchiveExtractorTest::testExtractRejectsUnsafeArchiveEntries() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString archivePath = tmp.filePath("unsafe.zip");
    QVERIFY(createArchiveWithUnsafePath(archivePath));

    ArchiveExtractor extractor;
    const ExtractionResult result = extractor.extract(archivePath, tmp.filePath("out"), false);

    QVERIFY(!result.success);
    QVERIFY(result.error.contains("unsafe", Qt::CaseInsensitive));
}

void ArchiveExtractorTest::testExtractRejectsMixedUnsafeArchiveWithoutWritingFiles() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString archivePath = tmp.filePath("mixed-unsafe.zip");
    QVERIFY(createArchiveWithOrderedEntries(archivePath,
        { { QStringLiteral("safe.bin"), QByteArray("safe") }, { QStringLiteral("../evil.bin"), QByteArray("evil") } }));

    ArchiveExtractor extractor;
    const QString outDir = tmp.filePath("out");
    const ExtractionResult result = extractor.extract(archivePath, outDir, false);

    QVERIFY(!result.success);
    QCOMPARE(result.filesExtracted, 0);
    QCOMPARE(result.failedFiles, 0);
    QVERIFY(!QFileInfo::exists(outDir + "/safe.bin"));
}

void ArchiveExtractorTest::testExtractContinuesBelowFailureThreshold() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString archivePath = tmp.filePath("partial.zip");
    QVERIFY(createArchiveWithOrderedEntries(archivePath,
        { { QStringLiteral("ok.bin"), QByteArray("ok") }, { QStringLiteral("bad1.bin"), QByteArray("bad1") },
            { QStringLiteral("bad2.bin"), QByteArray("bad2") } }));

    const QString outDir = tmp.filePath("out");
    QDir().mkpath(outDir + "/bad1.bin");
    QDir().mkpath(outDir + "/bad2.bin");

    ArchiveExtractor extractor;
    const ExtractionResult result = extractor.extract(archivePath, outDir, false);

    QVERIFY(result.success);
    QCOMPARE(result.filesExtracted, 1);
    QCOMPARE(result.failedFiles, 2);
    QVERIFY(QFileInfo::exists(outDir + "/ok.bin"));
}

void ArchiveExtractorTest::testExtractFailsAtOneToThreeFailureRatio() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString archivePath = tmp.filePath("ratio.zip");
    QVERIFY(createArchiveWithOrderedEntries(archivePath,
        { { QStringLiteral("ok.bin"), QByteArray("ok") }, { QStringLiteral("bad1.bin"), QByteArray("bad1") },
            { QStringLiteral("bad2.bin"), QByteArray("bad2") }, { QStringLiteral("bad3.bin"), QByteArray("bad3") } }));

    const QString outDir = tmp.filePath("out");
    QDir().mkpath(outDir + "/bad1.bin");
    QDir().mkpath(outDir + "/bad2.bin");
    QDir().mkpath(outDir + "/bad3.bin");

    ArchiveExtractor extractor;
    const ExtractionResult result = extractor.extract(archivePath, outDir, false);

    QVERIFY(!result.success);
    QCOMPARE(result.filesExtracted, 1);
    QCOMPARE(result.failedFiles, 3);
    QVERIFY(QFileInfo::exists(outDir + "/ok.bin"));
}

void ArchiveExtractorTest::testBatchExtractCanBeCancelledAfterFirstItem() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // Create two archives
    QMap<QString, QByteArray> files;
    files["x.bin"] = QByteArray("data");
    const QString arch1 = tmp.filePath("a1.zip");
    const QString arch2 = tmp.filePath("a2.zip");
    QVERIFY(createTestArchive(arch1, files, ArchiveFormat::ZIP));
    QVERIFY(createTestArchive(arch2, files, ArchiveFormat::ZIP));

    ArchiveExtractor extractor;
    const QString outDir = tmp.filePath("batch");

    // Connect to batchProgress and cancel after the first item
    QObject::connect(&extractor, &ArchiveExtractor::batchProgress, [&](int completed, int /*total*/) {
        if (completed == 1)
            extractor.cancel();
    });

    const QList<ExtractionResult> results = extractor.batchExtract({ arch1, arch2 }, outDir, false);

    QCOMPARE(results.size(), 1); // Second item was skipped due to cancel
}

void ArchiveExtractorTest::testExtractUnsupported() {
    ArchiveExtractor extractor;
    const ExtractionResult result = extractor.extract("/tmp/foo.xyz", "/tmp/out", false);
    QVERIFY(!result.success);
    QVERIFY(!result.error.isEmpty());
}

void ArchiveExtractorTest::testReadMemberPrefix() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString archivePath = tmp.filePath("prefix.zip");
    const QByteArray memberData(512, 'P');
    QMap<QString, QByteArray> files;
    files["data.bin"] = memberData;
    QVERIFY(createTestArchive(archivePath, files, ArchiveFormat::ZIP));

    ArchiveExtractor extractor;
    const qint64 prefixLen = 128;
    const QByteArray prefix = extractor.readMemberPrefix(archivePath, "data.bin", prefixLen);

    QCOMPARE(prefix.size(), static_cast<int>(prefixLen));
    QCOMPARE(prefix, memberData.left(static_cast<int>(prefixLen)));
}

QTEST_MAIN(ArchiveExtractorTest)
#include "test_archive_extractor.moc"
