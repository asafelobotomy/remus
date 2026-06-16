#pragma once

#include <QDateTime>
#include <QString>

#include "constants/engines.h"
#include "constants/file_types.h"

namespace Remus {

struct FileRecord {
    int id = 0;
    int libraryId = 0;
    QString originalPath;
    QString currentPath;
    QString filename;
    QString extension;
    qint64 fileSize = 0;
    bool isCompressed = false;
    QString archivePath;
    QString archiveInternalPath;
    int systemId = 0;
    QString crc32;
    QString md5;
    QString sha1;
    QString raMd5;
    bool hashCalculated = false;
    bool isPrimary = true;
    int parentFileId = 0;
    QString baseTitle;
    QString discSetKey;
    int discNumber = 0;
    QString fileType = Constants::FileTypes::OFFICIAL;
    bool isPatched = false;
    QString patchName;
    bool isProcessed = false;
    QString processingStatus = Constants::Engines::ProcessingStatus::UNPROCESSED;
    QDateTime lastModified;
    QDateTime scannedAt;
};

struct AppliedPatchRecord {
    int id = 0;
    QString basePath;
    QString outputPath;
    QString patchPath;
    QString patchFormat;
    QString baseTitle;
    QString patchName;
    QString fileType = Constants::FileTypes::HACK;
    QString sourceChecksum;
    QString targetChecksum;
    QString patchChecksum;
    QString baseCrc32;
    QString baseMd5;
    QString baseSha1;
    QString outputCrc32;
    QString outputMd5;
    QString outputSha1;
    QDateTime appliedAt;
};

struct ModInstallationRecord {
    int id = 0;
    int baseFileId = 0;
    int patchedFileId = 0;
    QString catalogModId;
    QString modTitle;
    QString modAuthor;
    QString modVersion;
    QString modType = Constants::FileTypes::HACK;
    QString patchFormat;
    QString patchUrl;
    QString patchSha1;
    QString sourceUrl;
    QDateTime installedAt;
};

struct ModCatalogCacheRecord {
    int id = 0;
    QString sourceUrl;
    QString etag;
    QDateTime fetchedAt;
    int modCount = 0;
};

struct MatchResult {
    int matchId = 0;
    int fileId = 0;
    int gameId = 0;
    int systemId = 0;
    QString matchMethod;
    float confidence = 0;
    bool isConfirmed = false;
    bool isRejected = false;
    QString gameTitle;
    QString publisher;
    QString developer;
    int releaseYear = 0;
    QString releaseDate;
    QString description;
    QString genre;
    QString players;
    QString region;
    float rating = 0.0f;
    float nameMatchScore = 0.0f;
};

} // namespace Remus