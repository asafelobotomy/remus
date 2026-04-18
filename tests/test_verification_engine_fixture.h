#pragma once

#include <QtTest/QtTest>

#include "../src/core/database.h"
#include "../src/core/verification_engine.h"

using namespace Remus;

QString writeDat(const QTemporaryDir &dir);
QString writePatchDat(const QTemporaryDir &dir);
int populateDb(Database &db,
               const QString &crc,
               const QString &md5 = QString(),
               const QString &sha1 = QString(),
               bool hashCalculated = true);

class VerificationEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void testImportDat();
    void testImportPatchDat();
    void testVerifyMatchingHash();
    void testVerifyOfficialDatFallsBackToMd5WhenPreferredHashMissing();
    void testVerifyMismatch();
    void testVerifyNotInDat();
    void testVerifyHashMissing();
    void testVerifySummary();
    void testHasDat();
    void testHasPatchDat();
    void testRemoveDat();
    void testRemovePatchDat();
    void testGetMissingGames();
    void testVerifyPatchedHashPromotesMetadata();
    void testExportReportCsv();
    void testExportReportJson();
    void testGetImportedDatsReturnsHeaders();
    void testVerifyLibraryWithSystemFilter();
};