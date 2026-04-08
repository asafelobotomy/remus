#pragma once

#include <QtTest/QtTest>
#include <QDir>
#include <QTemporaryDir>
#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "../src/core/archive_creator.h"
#include "../src/core/archive_extractor.h"
#include "../src/core/chd_converter.h"
#include "../src/core/rom_bundler.h"
#include "../src/core/database.h"

using namespace Remus;

inline bool romBundlerWriteAll(QFile &file, const QByteArray &data)
{
    return file.write(data) == data.size();
}

class RomBundlerTest : public QObject
{
    Q_OBJECT

protected:
    static FileRecord makeFileRecord(int id, const QString &path, const QString &filename = {})
    {
        FileRecord r;
        r.id           = id;
        r.currentPath  = path;
        r.filename     = filename.isEmpty() ? QFileInfo(path).fileName() : filename;
        r.isCompressed = false;
        r.crc32        = "AABBCCDD";
        r.md5          = "abc123";
        r.sha1         = "def456";
        return r;
    }

    static Database::MatchResult makeMatch(const QString &title = "Test Game")
    {
        Database::MatchResult m;
        m.gameTitle   = title;
        m.matchMethod = "CRC32";
        m.confidence  = 1.0f;
        m.isRejected  = false;
        return m;
    }

    static GameMetadata makeMetadata(const QString &title = "Test Game")
    {
        GameMetadata meta;
        meta.title  = title;
        meta.system = "Test System";
        return meta;
    }

    static bool writeFile(const QString &path, const QByteArray &data = "DUMMY ROM DATA")
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return false;
        if (!romBundlerWriteAll(f, data)) return false;
        f.close();
        return true;
    }

    static bool writeMinimalCueBinSet(const QString &cuePath, const QString &binPath)
    {
        if (!writeFile(binPath, QByteArray(2352 * 16, '\0')))
            return false;

        QFile cueFile(cuePath);
        if (!cueFile.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;

        const QByteArray cueContents = QStringLiteral(
            "FILE \"%1\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n")
            .arg(QFileInfo(binPath).fileName()).toUtf8();
        if (!romBundlerWriteAll(cueFile, cueContents))
            return false;
        cueFile.close();
        return true;
    }

    static bool copyFixtureDirectoryFiles(const QString &sourceDir, const QString &destinationDir)
    {
        const QDir dir(sourceDir);
        const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &entry : entries) {
            if (!QFile::copy(entry.absoluteFilePath(), destinationDir + "/" + entry.fileName()))
                return false;
        }
        return true;
    }

private slots:
    // ── isAlreadyBundled ──
    void testIsAlreadyBundled_nonexistentPath_returnsFalse();
    void testIsAlreadyBundled_plainFile_returnsFalse();
    // ── bundle() dry-run ──
    void testBundle_dryRun_returnsSuccessWithoutCreatingFile();
    void testBundle_dryRun_outputPathContainsBaseName();
    // ── bundle() real archives ──
    void testBundle_realZipContainsMarkerAndArtworkSubdir();
    void testBundle_realSevenZipContainsMarkerAndArtworkSubdir();
    void testBundle_compressedNestedPayloadIsFlattenedToArchiveRoot();
    void testBundle_markerUsesStoredPercentConfidence();
    void testBundle_skipsWhenCurrentCompressedPathAlreadyBundled();
    // ── disc conversion ──
    void testBundle_binPrimaryWithCueChildCanBePackagedAsChd();
    void testBundle_cueDiscMediaCanBePackagedAsChd();
    void testBundle_multiTrackGdiCanBePackagedAsChd();
    void testBundle_gameCubeIsoPrefersRvzWhenDiscOptimizationRequested();
    void testBundle_discConversionFailsWhenReferencedTrackIsMissing();
    // ── struct defaults ──
    void testBundleConfig_defaults();
    void testBundleResult_defaults();
};
