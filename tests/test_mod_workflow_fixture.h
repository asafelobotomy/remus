#pragma once

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlQuery>
#include <QUrl>

#include "../src/services/mod_catalog_provider.h"
#include "../src/services/mod_workflow_service.h"
#include "../src/core/database.h"

using namespace Remus;

inline bool writeAll(QFile &file, const QByteArray &data) {
    return file.write(data) == data.size();
}

inline bool removeIfExists(const QString &path) {
    return !QFile::exists(path) || QFile::remove(path);
}

class ModWorkflowTest : public QObject {
    Q_OBJECT

private:
    static QTemporaryDir s_modCatalogCacheRoot;

private slots:
    void initTestCase();

protected:
    QString catalogPath() const {
        const QStringList candidates = {
            QCoreApplication::applicationDirPath() + "/../../tests/fixtures/test_mod_catalog.json",
            QCoreApplication::applicationDirPath() + "/../tests/fixtures/test_mod_catalog.json",
            QCoreApplication::applicationDirPath() + "/../../../tests/fixtures/test_mod_catalog.json",
            QDir::currentPath() + "/tests/fixtures/test_mod_catalog.json",
            QDir::currentPath() + "/../tests/fixtures/test_mod_catalog.json",
            QString(REMUS_SOURCE_DIR) + "/tests/fixtures/test_mod_catalog.json",
        };
        for (const auto &path : candidates) {
            if (QFile::exists(path))
                return QDir::cleanPath(path);
        }
        return { };
    }

private slots:
    // ── ModCatalogProvider tests ──
    void loadCatalog_validJson();
    void loadCatalog_invalidJson();
    void loadCatalog_missingFile();
    void findModsForRom_hashMatch();
    void findModsForRom_crc32Match();
    void findModsForRom_noMatch();
    void findModsBySystem();
    void getModById();
    // ── Database mod_installations CRUD ──
    void dbModInstallation_insertAndQuery();
    void dbModInstallation_removeNonexistent();
    // ── ModWorkflowService install tests ──
    void install_patchVerificationFails();
    void install_missingPatchFile();
    void install_rollsBackWhenRecordingFails();
    // ── Phase 2: URL loading + cache tests ──
    void loadFromUrl_fetchesAndCaches();
    void loadFromUrl_usesCacheWhenFresh();
    void loadFromUrl_networkErrorFallsBackToCache();
    void loadFromUrl_networkErrorNoCache();
    void loadFromUrl_privateIpHostRejected();
    // ── Phase 2: DB catalog cache CRUD ──
    void dbCatalogCache_upsertAndQuery();
    void dbCatalogCache_queryMissing();
    void cacheDir_returnsValidPath();
    void cacheFileForUrl_deterministicHash();
    // ── Phase 3: Patch download + integrity verification ──
    void resolvePatchPath_fileUrl_resolves();
    void resolvePatchPath_fileUrl_rejectedFromRemoteCatalog();
    void resolvePatchPath_relativeUrl_rejectedFromRemoteCatalog();
    void resolvePatchPath_httpUrl_rejected();
    void resolvePatchPath_emptyUrl_fails();
    void resolvePatchPath_unsupportedScheme_fails();
    void resolvePatchPath_privateIpHostRejected();
    void verifySha1_correctHash();
    void downloadPatch_unreachableUrl_fails();
    void install_progressCallbackFires();
    void downloadDir_cleanedOnDestruction();
};
