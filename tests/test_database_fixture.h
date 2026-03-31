#pragma once

#include <QtTest/QtTest>
#include <QSqlQuery>

#include "../src/core/constants/constants.h"
#include "../src/core/database.h"

using namespace Remus;

class DatabaseTest : public QObject
{
    Q_OBJECT

private slots:
    void testInitializeInMemory();
    void testInitializeEnablesForeignKeysOnExistingDatabase();
    void testInsertAndGetFile();
    void testSystemIdsRemainStableAcrossReopen();
    void testInitializeRepairsDanglingSystemIds();
    void testInitializeRollsBackFailedMigrations();
    void testUpdateFileHashes();
    void testRemoveFile();
    void testInsertAndGetMatch();
    void testConfirmRejectMatch();
    void testInsertLibraryAndDelete();
    void testGetFilesBySystem();
    void testMarkFileProcessed();
    void testInsertGame();
    void testUpdateGame();
    void testGetFileCountBySystem();
    void testGetFilesWithoutHashes();
    void testGetUnprocessedFiles();
    void testUpdateFilePath();
    void testInsertAndGetPatchedFileMetadata();
    void testInsertAndFindAppliedPatch();
    void testUpdateFileHashesPromotesPatchedMetadata();
    void testDeleteFilesForLibrary();
    void testGetAllFilesIncludesStaleEntries();
    void testGetExistingFilesOnlyReturnsValidPaths();
    void testGetFilePath();
    void testUpdateFileOriginalPath();
    void testGetFilesByParent();
    void testGetAllMatches();
    void testInsertAndGetModInstallation();
    void testRemoveModInstallation();
    void testUpsertAndGetCatalogCache();
    void testDeleteLibraryCascadesFiles();
    void testInsertMatchWithNameMatchScore();
};

FileRecord makeRecord(int libId, int sysId, const QString &name);
bool execSql(QSqlQuery &query, const QString &sql);
bool createLegacyDatabaseWithBrokenAppliedPatches(const QString &dbPath);
bool tableHasColumn(const QString &dbPath, const QString &tableName, const QString &columnName);