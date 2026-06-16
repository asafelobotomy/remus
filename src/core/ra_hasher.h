#pragma once

#include <QString>

namespace Remus {

/**
 * @brief Computes RetroAchievements (RAHasher) MD5 digests for ROM files.
 *
 * RA hashes differ from No-Intro MD5 on several systems. Results must be stored
 * separately (FileRecord::raMd5) and used only for RetroAchievements lookups.
 */
class RaHasher {
public:
    struct Result {
        QString md5;
        bool success = false;
        QString error;
        bool usedExternalTool = false;
    };

    static Result compute(const QString &filePath, int remusSystemId, const QString &extension = QString());

    static bool hasRaMapping(int remusSystemId);
    static int raConsoleId(int remusSystemId);

    static void setExternalToolPath(const QString &path);
    static void setExternalSystemPath(const QString &path);

    /** @brief Test hook — returns the RA MD5 for raw payload using native rules only. */
    static QString md5ForPayload(
        const QByteArray &payload, int remusSystemId, const QString &extension, qint64 originalFileSize = -1);
};

} // namespace Remus
