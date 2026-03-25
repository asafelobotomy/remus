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
#include <QTimer>
#include <QUrl>
#include <QDebug>

#include "../core/archive_extractor.h"
#include "../core/constants/constants.h"
#include "../core/hasher.h"

namespace Remus {

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

    if (baseFile.isCompressed) {
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
            baseRomPath = tempExtractDir + "/" + baseFile.archiveInternalPath;
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
    patchedRecord.fileType       = mod.type.isEmpty() ? Constants::FileTypes::HACK : mod.type;
    patchedRecord.isPatched      = true;
    patchedRecord.patchName      = mod.title;

    int patchedFileId = m_db.insertFile(patchedRecord);
    if (patchedFileId <= 0) {
        result.error = "Failed to register patched file in database";
        return result;
    }
    result.patchedFileId = patchedFileId;

    // ── 8. Record in mod_installations ───────────────────────────────────────
    if (cb) cb("recording", 90);
    Database::ModInstallationRecord modRec;
    modRec.baseFileId    = baseFile.id;
    modRec.patchedFileId = patchedFileId;
    modRec.catalogModId  = mod.id;
    modRec.modTitle      = mod.title;
    modRec.modAuthor     = mod.author;
    modRec.modVersion    = mod.version;
    modRec.modType       = mod.type.isEmpty() ? Constants::FileTypes::HACK : mod.type;
    modRec.patchFormat   = mod.format;
    modRec.patchUrl      = mod.patchUrl;
    modRec.patchSha1     = mod.patchSha1;
    modRec.sourceUrl     = mod.sourceUrl;

    if (m_db.insertModInstallation(modRec) < 0) {
        result.error = "Patched file created but failed to record mod installation";
        // Partial success — patched file exists but installation not tracked
        return result;
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
            QFile::remove(patchedFile.currentPath);
        }
        // Remove from files table
        QSqlQuery delFile(m_db.database());
        delFile.prepare(QStringLiteral("DELETE FROM files WHERE id = ?"));
        delFile.addBindValue(patchedFileId);
        delFile.exec();
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
