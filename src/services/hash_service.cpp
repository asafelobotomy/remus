#include "hash_service.h"

#include "../core/hasher.h"
#include "../core/database.h"
#include "../core/archive_extractor.h"
#include "../core/chd_converter.h"
#include "../core/chd_header.h"
#include "../core/constants/files.h"
#include "../core/ra_hasher.h"
#include "../core/rvz_converter.h"

#include <QCryptographicHash>

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlDriver>
#include <QStorageInfo>
#include <QThread>
#include <QThreadPool>
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrentMap>
#include <atomic>
#include <memory>

namespace Remus {

namespace {

    // Return a temp-dir base path that has at least estimatedBytes free.
    // Search order: REMUS_TMPDIR env var → system temp → archive parent dir.
    // Returns empty string if none of the candidates have sufficient space.
    struct TempDirChoice {
        QString path;
        QString warningIfFallback; // non-empty when we fell back from system temp
    };

    TempDirChoice chooseTempBase(const QString &archivePath, qint64 estimatedBytes) {
        auto hasSpace = [](const QString &dir, qint64 needed) -> bool {
            QStorageInfo info(dir);
            return info.isValid() && info.bytesAvailable() >= needed;
        };

        // 1. User-specified override
        const QString override = qEnvironmentVariable("REMUS_TMPDIR");
        if (!override.isEmpty()) {
            if (hasSpace(override, estimatedBytes))
                return { override, { } };
        }

        // 2. System default temp
        const QString sysTemp = QDir::tempPath();
        if (hasSpace(sysTemp, estimatedBytes))
            return { sysTemp, { } };

        // 3. Archive's own parent directory as last resort
        const QString parentDir = QFileInfo(archivePath).absolutePath();
        if (hasSpace(parentDir, estimatedBytes)) {
            return {
                parentDir,
                QStringLiteral("System temp (%1) lacks space; extracting beside archive (%2)").arg(sysTemp, parentDir)
            };
        }

        // Nothing suitable found — return empty so the caller can emit a clear error
        QStorageInfo si(sysTemp);
        return { { },
            QStringLiteral("No temp location has ~%1 MB free. "
                           "Set REMUS_TMPDIR to a directory on a larger partition. "
                           "(%2 has %3 MB available)")
                .arg(estimatedBytes / (1024 * 1024))
                .arg(sysTemp)
                .arg(si.bytesAvailable() / (1024 * 1024)) };
    }

    QString selectExtractedMember(const QString &outputDir, const QStringList &extractedFiles,
        const QString &expectedMemberPath, const QString &fallbackFileName) {
        const QString normalizedMember = ArchiveExtractor::normalizeArchiveMemberPath(expectedMemberPath);
        const QString normalizedFallback = ArchiveExtractor::normalizeArchiveMemberPath(fallbackFileName);
        const QString preferredFileName
            = QFileInfo(!normalizedMember.isEmpty() ? normalizedMember : normalizedFallback).fileName();

        for (const QString &path : extractedFiles) {
            const QString relativePath = QDir(outputDir).relativeFilePath(path).replace('\\', '/');
            if (!normalizedMember.isEmpty() && relativePath == normalizedMember) {
                return path;
            }
        }

        QString matchedByName;
        for (const QString &path : extractedFiles) {
            if (QFileInfo(path).fileName().compare(preferredFileName, Qt::CaseInsensitive) == 0) {
                if (!matchedByName.isEmpty()) {
                    return QString();
                }
                matchedByName = path;
            }
        }

        return matchedByName;
    }

    void attachRaHash(HashResult &result, const QString &hashedPath, const FileRecord &file) {
        if (!result.success || file.systemId <= 0 || !RaHasher::hasRaMapping(file.systemId))
            return;

        const RaHasher::Result ra = RaHasher::compute(hashedPath, file.systemId, file.extension);
        if (ra.success)
            result.raMd5 = ra.md5;
    }

    void attachChdSha1(HashResult &result, const QString &path, const QString &extension) {
        if (!result.success || extension.trimmed().compare(Constants::Files::CHD, Qt::CaseInsensitive) != 0)
            return;

        CHDConverter chd;
        if (!chd.isChdmanAvailable())
            return;

        const CHDInfo info = chd.getCHDInfo(path);
        const QString discSha1 = info.hasheousDiscSha1();
        if (!discSha1.isEmpty())
            result.chdSha1 = discSha1;
    }

    void attachRvzSha1(HashResult &result, const QString &path, const QString &extension) {
        const QString ext = extension.trimmed().toLower();
        if (!result.success || (ext != Constants::Files::RVZ && ext != Constants::Files::GCZ))
            return;

        RVZConverter rvz;
        if (!rvz.isDolphinToolAvailable())
            return;

        const QString contentSha1 = rvz.discContentSha1(path);
        if (!contentSha1.isEmpty())
            result.rvzSha1 = contentSha1;
    }

    bool isArchiveRomMemberPath(const QString &memberPath) {
        const QString lower = memberPath.trimmed().toLower();
        if (lower.isEmpty())
            return false;

        static const QStringList kSkipExtensions = {
            QStringLiteral(".txt"),
            QStringLiteral(".nfo"),
            QStringLiteral(".diz"),
            QStringLiteral(".xml"),
            QStringLiteral(".htm"),
            QStringLiteral(".html"),
            QStringLiteral(".jpg"),
            QStringLiteral(".png"),
            QStringLiteral(".gif"),
            QStringLiteral(".bmp"),
            QStringLiteral(".pdf"),
            QStringLiteral(".exe"),
            QStringLiteral(".dll"),
            QStringLiteral(".so"),
            QStringLiteral(".md"),
        };
        for (const QString &ext : kSkipExtensions) {
            if (lower.endsWith(ext))
                return false;
        }
        return true;
    }

    QString compositeSha1FromMemberSha1s(const QStringList &sortedMemberSha1s) {
        QCryptographicHash hasher(QCryptographicHash::Sha1);
        for (const QString &memberSha1 : sortedMemberSha1s) {
            hasher.addData(memberSha1.trimmed().toLower().toUtf8());
        }
        return QString::fromLatin1(hasher.result().toHex());
    }

    void applyCompositeArchiveSha1(HashResult &result, const QStringList &sortedMemberSha1s) {
        if (!result.success || sortedMemberSha1s.size() < 2)
            return;
        result.sha1 = compositeSha1FromMemberSha1s(sortedMemberSha1s);
    }

} // namespace

HashService::HashService()
    : m_hasher(std::make_unique<Hasher>()) { }

HashService::~HashService() { }

int HashService::hashAll(
    Database *db, ProgressCallback progressCb, LogCallback logCb, const std::atomic<bool> *cancelled) {
    if (!db)
        return 0;

    QList<FileRecord> files = db->getFilesWithoutHashes();
    const int total = files.size();
    if (progressCb)
        progressCb(0, total, QString());

    if (total == 0) {
        if (logCb)
            logCb(QString("Hashing complete: 0/0"));
        return 0;
    }

    if (cancelled && cancelled->load()) {
        if (logCb)
            logCb(QString("Hashing cancelled: 0/%1").arg(total));
        return 0;
    }

    const QList<HashBatchResult> taskResults = computeHashes(files, progressCb, cancelled);

    int hashed = 0;
    int skipped = 0;

    QSqlDatabase sqlDb = db->database();
    bool useTransaction = sqlDb.driver()->hasFeature(QSqlDriver::Transactions);
    if (useTransaction && !sqlDb.transaction()) {
        qWarning() << "HashService::hashAll: failed to start transaction, proceeding without";
        useTransaction = false;
    }

    for (const HashBatchResult &task : taskResults) {
        if (task.skipped) {
            skipped++;
            continue;
        }
        if (task.result.success) {
            if (db->updateFileHashes(task.fileId, task.result.crc32, task.result.md5, task.result.sha1,
                    task.result.raMd5, task.result.chdSha1, task.result.rvzSha1)) {
                hashed++;
            } else {
                skipped++;
                if (logCb)
                    logCb(QString("Skipped %1: failed to persist hashes").arg(task.filename));
            }
        } else {
            skipped++;
            if (logCb)
                logCb(QString("Skipped %1: %2").arg(task.filename, task.result.error));
        }
    }

    if (useTransaction) {
        if (!sqlDb.commit()) {
            qWarning() << "HashService::hashAll: commit failed, rolling back";
            sqlDb.rollback();
            return 0;
        }
    }

    if (logCb) {
        if (skipped > 0)
            logCb(QString("Hashing complete: %1 hashed, %2 skipped").arg(hashed).arg(skipped));
        else
            logCb(QString("Hashing complete: %1/%2").arg(hashed).arg(total));
    }
    return hashed;
}

QList<HashService::HashBatchResult> HashService::computeHashes(
    const QList<FileRecord> &files, ProgressCallback progressCb, const std::atomic<bool> *cancelled) {
    const int total = files.size();
    if (total == 0)
        return { };

    const int idealThreads = QThread::idealThreadCount();
    const int maxThreads = qMax(1, qMin(idealThreads > 0 ? idealThreads : 1, 8));

    QThreadPool localPool;
    localPool.setMaxThreadCount(maxThreads);

    // Share an atomic completion counter so workers can emit live progress rather
    // than delivering all updates in a burst after blockingMapped() returns.
    auto doneCount = std::make_shared<std::atomic<int>>(0);
    ProgressCallback cbCopy = progressCb;

    QList<HashBatchResult> taskResults = QtConcurrent::blockingMapped(
        &localPool, files, [total, doneCount, cbCopy, cancelled](const FileRecord &file) {
            HashBatchResult task;
            task.fileId = file.id;
            task.filename = file.filename;

            if (cancelled && cancelled->load()) {
                task.skipped = true;
                task.skipReason = QStringLiteral("cancelled");
            } else {
                HashService worker;
                task.result = worker.hashRecord(file);
            }

            if (cbCopy) {
                const int done = ++(*doneCount);
                cbCopy(done, total, task.filename);
            }

            return task;
        });

    return taskResults;
}

bool HashService::hashFile(Database *db, int fileId) {
    if (!db)
        return false;

    FileRecord file = db->getFileById(fileId);
    if (file.id == 0)
        return false;

    HashResult result = hashRecord(file);
    if (result.success) {
        db->updateFileHashes(
            file.id, result.crc32, result.md5, result.sha1, result.raMd5, result.chdSha1, result.rvzSha1);
        return true;
    }
    return false;
}

HashResult HashService::hashRecord(const FileRecord &file) {
    // Detect whether this file is inside an archive
    auto isArchivePath = [](const QString &path) {
        const QString lower = path.toLower();
        for (const QString &extension : Constants::Files::ARCHIVE_EXTENSIONS) {
            if (lower.endsWith(extension)) {
                return true;
            }
        }
        return false;
    };

    const QString archivePath = file.archivePath.isEmpty() ? file.currentPath : file.archivePath;
    const bool treatAsArchive = file.isCompressed || isArchivePath(archivePath);

    if (!treatAsArchive) {
        int headerSize = Hasher::detectHeaderSize(file.currentPath, file.extension);
        HashResult result = m_hasher->calculateHashes(file.currentPath, headerSize > 0, headerSize);
        attachRaHash(result, file.currentPath, file);
        attachChdSha1(result, file.currentPath, file.extension);
        attachRvzSha1(result, file.currentPath, file.extension);
        return result;
    }

    // Archive-aware hashing: extract to temp dir, then hash
    HashResult result;
    QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists()) {
        result.error = "Archive file not found: " + archivePath;
        return result;
    }

    // Archive disc images can expand significantly from compressed form.
    // Use 4× the compressed file size as a conservative space estimate.
    const qint64 estimatedBytes = qMax(archiveInfo.size() * 4, qint64(512 * 1024 * 1024));
    const TempDirChoice tempChoice = chooseTempBase(archivePath, estimatedBytes);

    if (tempChoice.path.isEmpty()) {
        result.error = tempChoice.warningIfFallback;
        return result;
    }

    if (!tempChoice.warningIfFallback.isEmpty())
        qWarning() << "remus.hash:" << tempChoice.warningIfFallback;

    QTemporaryDir tempDir(tempChoice.path + QStringLiteral("/remus-hash-XXXXXX"));
    if (!tempDir.isValid()) {
        result.error = QStringLiteral("Failed to create temporary directory in %1: %2")
                           .arg(tempChoice.path, tempDir.errorString());
        return result;
    }

    ArchiveExtractor extractor;
    const ArchiveInfo archiveMeta = extractor.getArchiveInfo(archivePath);
    QStringList romMembers;
    romMembers.reserve(archiveMeta.contents.size());
    for (const QString &member : archiveMeta.contents) {
        if (isArchiveRomMemberPath(member))
            romMembers.append(member);
    }
    romMembers.sort(Qt::CaseSensitive);

    if (romMembers.size() >= 2 && file.archiveInternalPath.isEmpty()) {
        ExtractionResult extraction = extractor.extract(archivePath, tempDir.path(), false);
        if (!extraction.success || extraction.extractedFiles.isEmpty()) {
            result.error = extraction.error.isEmpty()
                ? QStringLiteral("Failed to extract archive for composite hashing: %1").arg(archivePath)
                : extraction.error;
            return result;
        }

        QStringList memberSha1s;
        HashResult largestResult;
        qint64 largestSize = -1;
        QString largestPath;

        for (const QString &member : romMembers) {
            const QString picked = selectExtractedMember(
                tempDir.path(), extraction.extractedFiles, member, QFileInfo(member).fileName());
            if (picked.isEmpty())
                continue;

            const int headerSize = Hasher::detectHeaderSize(picked, QFileInfo(member).suffix());
            const HashResult memberResult = m_hasher->calculateHashes(picked, headerSize > 0, headerSize);
            if (!memberResult.success)
                continue;

            memberSha1s.append(memberResult.sha1);
            const qint64 memberSize = QFileInfo(picked).size();
            if (memberSize > largestSize) {
                largestSize = memberSize;
                largestResult = memberResult;
                largestPath = picked;
            }
        }

        if (memberSha1s.size() >= 2 && largestResult.success) {
            result = largestResult;
            applyCompositeArchiveSha1(result, memberSha1s);
            if (!largestPath.isEmpty()) {
                attachRaHash(result, largestPath, file);
                attachChdSha1(result, largestPath, file.extension);
                attachRvzSha1(result, largestPath, file.extension);
            }
            return result;
        }
    }

    const QString internalPath = file.archiveInternalPath.isEmpty() ? file.filename : file.archiveInternalPath;

    ExtractionResult extraction = extractor.extractFile(archivePath, internalPath, tempDir.path());

    if (!extraction.success || extraction.extractedFiles.isEmpty()) {
        // Fallback: extract entire archive and pick suitable file
        extraction = extractor.extract(archivePath, tempDir.path(), false);
        if (!extraction.success || extraction.extractedFiles.isEmpty()) {
            result.error = extraction.error.isEmpty() ? QString("Failed to extract %1 from archive").arg(internalPath)
                                                      : extraction.error;
            return result;
        }

        const QString picked
            = selectExtractedMember(tempDir.path(), extraction.extractedFiles, internalPath, file.filename);

        if (picked.isEmpty()) {
            result.error = QStringLiteral("Failed to locate extracted archive member: %1").arg(internalPath);
            return result;
        }

        int headerSize = Hasher::detectHeaderSize(picked, file.extension);
        result = m_hasher->calculateHashes(picked, headerSize > 0, headerSize);
        attachRaHash(result, picked, file);
        attachChdSha1(result, picked, file.extension);
        attachRvzSha1(result, picked, file.extension);
        return result;
    }

    const QString extractedPath = extraction.extractedFiles.first();
    int headerSize = Hasher::detectHeaderSize(extractedPath, file.extension);
    result = m_hasher->calculateHashes(extractedPath, headerSize > 0, headerSize);
    attachRaHash(result, extractedPath, file);
    attachChdSha1(result, extractedPath, file.extension);
    attachRvzSha1(result, extractedPath, file.extension);
    return result;
}

QString HashService::chdDiscSha1ForPath(const QString &chdPath) {
    if (chdPath.trimmed().isEmpty())
        return QString();

    const ChdHeaderDigest nativeDigest = readChdHeaderDigest(chdPath);
    if (nativeDigest.valid)
        return nativeDigest.sha1;

    CHDConverter chd;
    if (!chd.isChdmanAvailable())
        return QString();

    return chd.getCHDInfo(chdPath).hasheousDiscSha1();
}

int HashService::backfillChdSha1(Database *db, ProgressCallback progressCb, LogCallback logCb,
    const std::atomic<bool> *cancelled, const QSet<int> *fileScopeIds) {
    if (!db)
        return 0;

    CHDConverter chd;
    const bool chdmanAvailable = chd.isChdmanAvailable();
    if (!chdmanAvailable && logCb) {
        logCb(QStringLiteral("CHD backfill: chdman not found; using in-process CHD v5 header reads where possible"));
    }

    QList<FileRecord> files = db->getFilesNeedingChdSha1();
    const int total = files.size();
    if (total == 0) {
        if (logCb)
            logCb(QStringLiteral("CHD backfill: no .chd files need header SHA1"));
        return 0;
    }

    int updated = 0;
    int skipped = 0;
    int done = 0;

    for (const FileRecord &file : files) {
        if (cancelled && cancelled->load()) {
            if (logCb)
                logCb(QStringLiteral("CHD backfill cancelled at %1/%2").arg(done).arg(total));
            break;
        }

        if (fileScopeIds != nullptr && !fileScopeIds->isEmpty() && !fileScopeIds->contains(file.id))
            continue;

        ++done;
        if (progressCb)
            progressCb(done, total, file.filename);

        if (file.isCompressed || !file.archivePath.isEmpty()) {
            ++skipped;
            if (logCb)
                logCb(QStringLiteral("Skipped %1: CHD inside archive (extract first)").arg(file.filename));
            continue;
        }

        const QString discSha1 = chdDiscSha1ForPath(file.currentPath);
        if (discSha1.isEmpty()) {
            ++skipped;
            if (logCb)
                logCb(QStringLiteral("Skipped %1: could not read CHD header SHA1").arg(file.filename));
            continue;
        }

        if (db->updateFileChdSha1(file.id, discSha1))
            ++updated;
        else
            ++skipped;
    }

    if (logCb) {
        if (skipped > 0)
            logCb(QStringLiteral("CHD backfill complete: %1 updated, %2 skipped").arg(updated).arg(skipped));
        else
            logCb(QStringLiteral("CHD backfill complete: %1 updated").arg(updated));
    }

    return updated;
}

QString HashService::rvzDiscContentSha1ForPath(const QString &discPath) {
    if (discPath.trimmed().isEmpty())
        return QString();

    RVZConverter rvz;
    if (!rvz.isDolphinToolAvailable())
        return QString();

    return rvz.discContentSha1(discPath);
}

int HashService::backfillRvzSha1(Database *db, ProgressCallback progressCb, LogCallback logCb,
    const std::atomic<bool> *cancelled, const QSet<int> *fileScopeIds) {
    if (!db)
        return 0;

    RVZConverter rvz;
    if (!rvz.isDolphinToolAvailable()) {
        if (logCb)
            logCb(QStringLiteral("RVZ backfill skipped: dolphin-tool not found"));
        return 0;
    }

    QList<FileRecord> files = db->getFilesNeedingRvzSha1();
    const int total = files.size();
    if (total == 0) {
        if (logCb)
            logCb(QStringLiteral("RVZ backfill: no .rvz/.gcz files need content SHA1"));
        return 0;
    }

    int updated = 0;
    int skipped = 0;
    int done = 0;

    for (const FileRecord &file : files) {
        if (cancelled && cancelled->load()) {
            if (logCb)
                logCb(QStringLiteral("RVZ backfill cancelled at %1/%2").arg(done).arg(total));
            break;
        }

        if (fileScopeIds != nullptr && !fileScopeIds->isEmpty() && !fileScopeIds->contains(file.id))
            continue;

        ++done;
        if (progressCb)
            progressCb(done, total, file.filename);

        if (file.isCompressed || !file.archivePath.isEmpty()) {
            ++skipped;
            if (logCb)
                logCb(QStringLiteral("Skipped %1: archive-contained RVZ/GCZ not supported for backfill")
                        .arg(file.filename));
            continue;
        }

        const QString contentSha1 = rvzDiscContentSha1ForPath(file.currentPath);
        if (contentSha1.isEmpty()) {
            ++skipped;
            if (logCb)
                logCb(QStringLiteral("Skipped %1: dolphin-tool could not read content SHA1").arg(file.filename));
            continue;
        }

        if (db->updateFileRvzSha1(file.id, contentSha1))
            ++updated;
        else
            ++skipped;
    }

    if (logCb) {
        if (skipped > 0)
            logCb(QStringLiteral("RVZ backfill complete: %1 updated, %2 skipped").arg(updated).arg(skipped));
        else
            logCb(QStringLiteral("RVZ backfill complete: %1 updated").arg(updated));
    }

    return updated;
}

} // namespace Remus
