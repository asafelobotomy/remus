#include "mod_workflow_service.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDriver>
#include <QTimer>
#include <QUrl>
#include <QDebug>

#include "../core/archive_extractor.h"
#include "../core/constants/constants.h"
#include "../core/hasher.h"

namespace Remus {

namespace {

bool shouldTreatAsArchive(const FileRecord &file)
{
    const QString archiveCandidate = file.archivePath.isEmpty() ? file.currentPath : file.archivePath;
    const QString lower = archiveCandidate.toLower();

    if (file.isCompressed || !file.archiveInternalPath.isEmpty()) {
        return true;
    }

    for (const QString &extension : Constants::Files::ARCHIVE_EXTENSIONS) {
        if (lower.endsWith(extension)) {
            return true;
        }
    }

    return false;
}

}

ModWorkflowService::ModWorkflowService(Database &db, PatchService &patchService)
    : m_db(db)
    , m_patchSvc(patchService)
{
}

// ── Install ──────────────────────────────────────────────────────────────────

ModInstallResult ModWorkflowService::install(const FileRecord  &baseFile,
                                             const ModEntry     &mod,
                                             const QString      &outputDir,
                                             ProgressCallback    cb)
{
    ModInstallResult result;
    const QString modType = mod.type.trimmed().toLower();

    if (modType.isEmpty()) {
        result.error = "Catalog mod type is required";
        return result;
    }

    const bool validModType = modType == Constants::FileTypes::HACK ||
        modType == Constants::FileTypes::TRANSLATION ||
        modType == Constants::FileTypes::IMPROVEMENT ||
        modType == Constants::FileTypes::HOMEBREW ||
        modType == Constants::FileTypes::PROTOTYPE ||
        modType == Constants::FileTypes::OFFICIAL;
    if (!validModType) {
        result.error = "Catalog mod type is invalid: " + mod.type;
        return result;
    }

    if (cb) cb("resolving", 0);

    // ── 1. Resolve patch file path (may download from HTTP) ──────────────────
    QString resolveError;
    const QString patchPath = resolvePatchPath(mod.patchUrl, resolveError, cb);
    if (patchPath.isEmpty()) {
        result.error = "Cannot resolve patch: " + resolveError;
        return result;
    }

    // ── 2. Verify patch SHA1 ─────────────────────────────────────────────────
    if (cb) cb("verifying", 15);
    if (!mod.patchSha1.isEmpty() && !verifySha1(patchPath, mod.patchSha1)) {
        result.error = "Patch SHA1 mismatch — file may be corrupted or tampered with";
        return result;
    }

    // ── 3. Resolve base ROM path ─────────────────────────────────────────────
    if (cb) cb("preparing", 20);
    QString baseRomPath;
    QString tempExtractDir;

    if (shouldTreatAsArchive(baseFile)) {
        // Extract to a temp directory
        tempExtractDir = QDir::tempPath() + "/.remus_mod_"
                       + QString::number(QDateTime::currentMSecsSinceEpoch());
        QDir().mkpath(tempExtractDir);

        ArchiveExtractor extractor;
        const QString archivePath = baseFile.currentPath.isEmpty()
                                    ? baseFile.archivePath : baseFile.currentPath;
        ExtractionResult ex = extractor.extract(archivePath, tempExtractDir, false);
        if (!ex.success) {
            result.error = "Failed to extract base ROM: " + ex.error;
            QDir(tempExtractDir).removeRecursively();
            return result;
        }

        if (!baseFile.archiveInternalPath.isEmpty()) {
            const QString normalizedInternalPath =
                ArchiveExtractor::normalizeArchiveMemberPath(baseFile.archiveInternalPath);
            if (normalizedInternalPath.isEmpty()) {
                result.error = "Base ROM archive path is unsafe";
                QDir(tempExtractDir).removeRecursively();
                return result;
            }

            baseRomPath = QDir(tempExtractDir).filePath(normalizedInternalPath);
        } else if (!ex.extractedFiles.isEmpty()) {
            baseRomPath = ex.extractedFiles.first();
        }
    } else {
        baseRomPath = baseFile.currentPath;
    }

    if (baseRomPath.isEmpty() || !QFile::exists(baseRomPath)) {
        result.error = "Base ROM file not found";
        if (!tempExtractDir.isEmpty()) QDir(tempExtractDir).removeRecursively();
        return result;
    }

    // ── 4. Determine output path ─────────────────────────────────────────────
    QDir outDir(outputDir);
    if (!outDir.exists() && !outDir.mkpath(".")) {
        result.error = "Cannot create output directory: " + outputDir;
        if (!tempExtractDir.isEmpty()) QDir(tempExtractDir).removeRecursively();
        return result;
    }

    const QString baseName = QFileInfo(baseFile.filename).completeBaseName();
    const QString ext      = QFileInfo(baseFile.filename).suffix();
    const QString patchedName = QString("%1 [%2].%3").arg(baseName, mod.title, ext);
    const QString patchedPath = outDir.absoluteFilePath(patchedName);

    // ── 5. Apply patch ───────────────────────────────────────────────────────
    if (cb) cb("patching", 40);
    PatchResult patchResult = m_patchSvc.apply(baseRomPath, patchPath, patchedPath);

    // Clean up temp extraction
    if (!tempExtractDir.isEmpty()) QDir(tempExtractDir).removeRecursively();

    if (!patchResult.success) {
        result.error = "Patch failed: " + patchResult.error;
        return result;
    }

    result.patchedRomPath = patchedPath;

    // ── 6. Calculate hashes for the patched file ─────────────────────────────
    if (cb) cb("hashing", 70);
    Hasher hasher;
    HashResult hashes = hasher.calculateHashes(patchedPath);

    // ── 7. Register patched ROM in the files table ───────────────────────────
    if (cb) cb("registering", 80);
    FileRecord patchedRecord;
    patchedRecord.libraryId      = baseFile.libraryId;
    patchedRecord.originalPath   = patchedPath;
    patchedRecord.currentPath    = patchedPath;
    patchedRecord.filename       = patchedName;
    patchedRecord.extension      = ext;
    patchedRecord.fileSize       = QFileInfo(patchedPath).size();
    patchedRecord.isCompressed   = false;
    patchedRecord.systemId       = baseFile.systemId;
    patchedRecord.crc32          = hashes.crc32;
    patchedRecord.md5            = hashes.md5;
    patchedRecord.sha1           = hashes.sha1;
    patchedRecord.hashCalculated = hashes.success;
    patchedRecord.isPrimary      = false;
    patchedRecord.parentFileId   = baseFile.id;
    patchedRecord.baseTitle      = baseName;
    patchedRecord.fileType       = modType;
    patchedRecord.isPatched      = true;
    patchedRecord.patchName      = mod.title;

    // ── 8. Record in mod_installations ───────────────────────────────────────
    if (cb) cb("recording", 90);
    Database::ModInstallationRecord modRec;
    modRec.baseFileId    = baseFile.id;
    modRec.catalogModId  = mod.id;
    modRec.modTitle      = mod.title;
    modRec.modAuthor     = mod.author;
    modRec.modVersion    = mod.version;
    modRec.modType       = modType;
    modRec.patchFormat   = mod.format;
    modRec.patchUrl      = mod.patchUrl;
    modRec.patchSha1     = mod.patchSha1;
    modRec.sourceUrl     = mod.sourceUrl;

    QSqlDatabase &db = m_db.database();
    const bool useTransaction = db.driver() && db.driver()->hasFeature(QSqlDriver::Transactions);
    if (useTransaction && !db.transaction()) {
        QFile::remove(patchedPath);
        result.error = "Failed to start install transaction: " + db.lastError().text();
        return result;
    }

    const auto rollbackInstall = [&](const QString &errorMessage) {
        if (useTransaction && !db.rollback()) {
            qWarning() << "Failed to roll back mod install transaction:" << db.lastError().text();
        }
        m_db.removeFile(result.patchedFileId);
        QFile::remove(patchedPath);
        result.patchedFileId = 0;
        result.error = errorMessage;
        return result;
    };

    int patchedFileId = m_db.insertFile(patchedRecord);
    if (patchedFileId <= 0) {
        return rollbackInstall("Failed to register patched file in database");
    }
    result.patchedFileId = patchedFileId;
    modRec.patchedFileId = patchedFileId;

    if (m_db.insertModInstallation(modRec) < 0) {
        return rollbackInstall("Patched file created but failed to record mod installation");
    }

    if (useTransaction && !db.commit()) {
        return rollbackInstall("Failed to commit mod installation: " + db.lastError().text());
    }

    if (cb) cb("done", 100);
    result.success = true;
    return result;
}

// ── Query ────────────────────────────────────────────────────────────────────

QList<Database::ModInstallationRecord> ModWorkflowService::getInstalledMods(int baseFileId)
{
    return m_db.getModInstallations(baseFileId);
}

// ── Uninstall ────────────────────────────────────────────────────────────────

bool ModWorkflowService::uninstall(int modInstallationId)
{
    // Look up the installation to find the patched file
    // We query all installations and find the matching one
    // (A targeted query would be better, but keeping it simple for Phase 1)
    QSqlQuery query(m_db.database());
    query.prepare(QStringLiteral(
        "SELECT patched_file_id FROM mod_installations WHERE id = ?"));
    query.addBindValue(modInstallationId);

    if (!query.exec() || !query.next()) {
        return false;
    }

    const int patchedFileId = query.value(0).toInt();

    // Delete the patched file from disk
    if (patchedFileId > 0) {
        FileRecord patchedFile = m_db.getFileById(patchedFileId);
        if (!patchedFile.currentPath.isEmpty() && QFile::exists(patchedFile.currentPath)) {
            if (!QFile::remove(patchedFile.currentPath)) {
                qWarning() << "Failed to delete patched file from disk:" << patchedFile.currentPath;
                return false;
            }
        }
        if (!m_db.removeFile(patchedFileId)) {
            qWarning() << "Failed to delete patched file record for ID:" << patchedFileId;
            return false;
        }
    }

    // Remove the mod_installations record
    return m_db.removeModInstallation(modInstallationId);
}

// ── Private helpers ──────────────────────────────────────────────────────────

QString ModWorkflowService::resolvePatchPath(const QString &patchUrl,
                                              QString &error,
                                              ProgressCallback cb)
{
    if (patchUrl.isEmpty()) {
        error = "Empty patch URL";
        return {};
    }

    QUrl url(patchUrl);

    // Handle file:// URLs
    if (url.scheme() == QStringLiteral("file")) {
        const QString localPath = url.toLocalFile();
        if (!QFile::exists(localPath)) {
            error = "Local patch file not found: " + localPath;
            return {};
        }
        return localPath;
    }

    // Handle plain local paths (no scheme)
    if (url.scheme().isEmpty() || url.isRelative()) {
        if (!QFile::exists(patchUrl)) {
            error = "Patch file not found: " + patchUrl;
            return {};
        }
        return patchUrl;
    }

    // HTTP(S) URLs — download to temp directory
    if (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https")) {
        return downloadPatch(url, error, cb);
    }

    error = "Unsupported URL scheme: " + url.scheme();
    return {};
}

QString ModWorkflowService::downloadPatch(const QUrl &url,
                                           QString &error,
                                           ProgressCallback cb)
{
    if (cb) cb("downloading", 2);

    // Create a temp directory that lives for the duration of the service
    if (!m_downloadDir) {
        m_downloadDir = std::make_unique<QTemporaryDir>();
        if (!m_downloadDir->isValid()) {
            error = "Failed to create temp directory for patch download";
            return {};
        }
    }

    // Derive filename from URL path, falling back to a generic name
    QString filename = QFileInfo(url.path()).fileName();
    if (filename.isEmpty()) {
        filename = QStringLiteral("patch.bin");
    }
    const QString destPath = m_downloadDir->path() + QStringLiteral("/") + filename;

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, Constants::API::USER_AGENT);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = manager.get(request);

    // Forward download progress to the install callback
    if (cb) {
        QObject::connect(reply, &QNetworkReply::downloadProgress,
            [&cb](qint64 received, qint64 total) {
                if (total > 0) {
                    const int pct = 2 + static_cast<int>(received * 10 / total); // 2-12%
                    cb("downloading", pct);
                }
            });
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(Constants::Network::ARTWORK_TIMEOUT_MS);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start();
    loop.exec();

    if (!timeout.isActive()) {
        // Abort timed-out patch downloads so they don't continue in the background
        reply->abort();
        reply->deleteLater();
        error = "Patch download timed out: " + url.toString();
        return {};
    }
    timeout.stop();

    if (reply->error() != QNetworkReply::NoError) {
        error = "Patch download failed: " + reply->errorString();
        reply->deleteLater();
        return {};
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (data.isEmpty()) {
        error = "Downloaded patch file is empty";
        return {};
    }

    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly)) {
        error = "Failed to write downloaded patch: " + destPath;
        return {};
    }
    file.write(data);
    file.close();

    if (cb) cb("downloaded", 12);
    return destPath;
}

bool ModWorkflowService::verifySha1(const QString &filePath, const QString &expectedSha1)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&file);
    const QString actual = QString::fromLatin1(hash.result().toHex());

    return actual.compare(expectedSha1, Qt::CaseInsensitive) == 0;
}

} // namespace Remus
