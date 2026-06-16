#include "ra_hasher.h"

#include "constants/providers.h"
#include "constants/system_ids.h"
#include "system_resolver.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>

namespace Remus {

namespace {

    using namespace Constants;
    using namespace Constants::Providers;
    using namespace Constants::Systems;

    QString s_externalToolPath;
    QString s_externalSystemPath;

    enum class RaHashMode { Unsupported, FullFile, Nes, Snes, N64, ExternalOnly };

    QString md5Hex(const QByteArray &data) {
        return QString(QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex()).toLower();
    }

    QString md5HexSkipping(const QString &filePath, qint64 skipBytes) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly))
            return { };

        if (skipBytes > 0 && !file.seek(skipBytes))
            return { };

        QCryptographicHash hash(QCryptographicHash::Md5);
        static constexpr qint64 kChunkSize = 65536;
        QByteArray chunk(kChunkSize, Qt::Uninitialized);
        while (!file.atEnd()) {
            const qint64 bytesRead = file.read(chunk.data(), kChunkSize);
            if (bytesRead <= 0)
                break;
            hash.addData(QByteArrayView(chunk.constData(), bytesRead));
        }
        return QString(hash.result().toHex()).toLower();
    }

    int raNesHeaderSkip(const QByteArray &header) {
        if (header.size() >= 4 && header[0] == 'N' && header[1] == 'E' && header[2] == 'S'
            && static_cast<unsigned char>(header[3]) == 0x1A) {
            return 16;
        }
        return 0;
    }

    int raSnesHeaderSkip(qint64 fileSize) {
        if (fileSize > 512 && (fileSize % 8192) == 512)
            return 512;
        return 0;
    }

    QByteArray normalizeN64Payload(const QByteArray &data, const QString &extension) {
        const QString ext = extension.toLower();
        if (ext == QLatin1String(".v64")) {
            QByteArray out = data;
            for (int i = 0; i + 1 < out.size(); i += 2)
                qSwap(out[i], out[i + 1]);
            return out;
        }
        if (ext == QLatin1String(".n64")) {
            QByteArray out = data;
            for (int i = 0; i + 3 < out.size(); i += 4) {
                qSwap(out[i], out[i + 3]);
                qSwap(out[i + 1], out[i + 2]);
            }
            return out;
        }
        return data;
    }

    RaHashMode hashModeForSystem(int systemId) {
        switch (systemId) {
        case ID_NES:
            return RaHashMode::Nes;
        case ID_SNES:
            return RaHashMode::Snes;
        case ID_N64:
            return RaHashMode::N64;
        case ID_PSX:
        case ID_PS2:
        case ID_GAMECUBE:
        case ID_WII:
        case ID_NDS:
        case ID_3DS:
        case ID_PSP:
        case ID_PSVITA:
        case ID_SATURN:
        case ID_DREAMCAST:
        case ID_SEGA_CD:
        case ID_ARCADE:
            return RaHashMode::ExternalOnly;
        default:
            if (RaHasher::hasRaMapping(systemId))
                return RaHashMode::FullFile;
            return RaHashMode::Unsupported;
        }
    }

    QString resolvedExternalToolPath() {
        if (!s_externalToolPath.isEmpty())
            return s_externalToolPath;
        return qEnvironmentVariable("REMUS_RAHASHER_PATH");
    }

    QString resolvedExternalSystemPath() {
        if (!s_externalSystemPath.isEmpty())
            return s_externalSystemPath;
        return qEnvironmentVariable("REMUS_RAHASHER_SYSTEM_PATH");
    }

    RaHasher::Result computeViaExternalTool(const QString &filePath, int consoleId) {
        RaHasher::Result result;
        const QString toolPath = resolvedExternalToolPath();
        if (toolPath.isEmpty()) {
            result.error = QStringLiteral("RAHasher binary not configured");
            return result;
        }

        QStringList args;
        const QString systemPath = resolvedExternalSystemPath();
        if (!systemPath.isEmpty()) {
            args << QStringLiteral("-s") << systemPath;
        }
        args << QString::number(consoleId) << filePath;

        QProcess process;
        process.start(toolPath, args);
        if (!process.waitForStarted(5000)) {
            result.error = QStringLiteral("Failed to start RAHasher: %1").arg(toolPath);
            return result;
        }
        if (!process.waitForFinished(120000)) {
            process.kill();
            process.waitForFinished(3000);
            result.error = QStringLiteral("RAHasher timed out");
            return result;
        }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            result.error = QString::fromUtf8(process.readAllStandardError()).trimmed();
            if (result.error.isEmpty())
                result.error = QStringLiteral("RAHasher exited with code %1").arg(process.exitCode());
            return result;
        }

        const QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        const QString token = output.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).value(0);
        if (token.size() != 32) {
            result.error = QStringLiteral("RAHasher returned unexpected output: %1").arg(output.left(80));
            return result;
        }

        result.md5 = token.toLower();
        result.success = true;
        result.usedExternalTool = true;
        return result;
    }

    RaHasher::Result computeNative(const QString &filePath, RaHashMode mode, const QString &extension) {
        RaHasher::Result result;
        const QFileInfo info(filePath);
        if (!info.exists() || !info.isFile()) {
            result.error = QStringLiteral("File not found: %1").arg(filePath);
            return result;
        }

        switch (mode) {
        case RaHashMode::FullFile:
            result.md5 = md5HexSkipping(filePath, 0);
            break;
        case RaHashMode::Nes: {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                result.error = QStringLiteral("Failed to open file: %1").arg(filePath);
                return result;
            }
            const QByteArray header = file.read(16);
            file.close();
            result.md5 = md5HexSkipping(filePath, raNesHeaderSkip(header));
            break;
        }
        case RaHashMode::Snes:
            result.md5 = md5HexSkipping(filePath, raSnesHeaderSkip(info.size()));
            break;
        case RaHashMode::N64: {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                result.error = QStringLiteral("Failed to open file: %1").arg(filePath);
                return result;
            }
            const QByteArray data = file.readAll();
            file.close();
            result.md5
                = md5Hex(normalizeN64Payload(data, extension.isEmpty() ? info.suffix().prepend('.') : extension));
            break;
        }
        default:
            result.error = QStringLiteral("Unsupported native RA hash mode");
            return result;
        }

        if (result.md5.size() != 32) {
            result.error = QStringLiteral("Failed to compute RA MD5");
            result.md5.clear();
            return result;
        }

        result.success = true;
        return result;
    }

} // namespace

void RaHasher::setExternalToolPath(const QString &path) {
    s_externalToolPath = path;
}

void RaHasher::setExternalSystemPath(const QString &path) {
    s_externalSystemPath = path;
}

bool RaHasher::hasRaMapping(int remusSystemId) {
    if (remusSystemId <= 0)
        return false;
    return !SystemResolver::providerName(remusSystemId, RETROACHIEVEMENTS).isEmpty();
}

int RaHasher::raConsoleId(int remusSystemId) {
    if (remusSystemId <= 0)
        return 0;
    bool ok = false;
    const int id = SystemResolver::providerName(remusSystemId, RETROACHIEVEMENTS).toInt(&ok);
    return ok ? id : 0;
}

QString RaHasher::md5ForPayload(
    const QByteArray &payload, int remusSystemId, const QString &extension, qint64 originalFileSize) {
    const RaHashMode mode = hashModeForSystem(remusSystemId);
    switch (mode) {
    case RaHashMode::FullFile:
        return md5Hex(payload);
    case RaHashMode::Nes: {
        const int skip = raNesHeaderSkip(payload.left(16));
        return md5Hex(payload.mid(skip));
    }
    case RaHashMode::Snes: {
        const qint64 size = originalFileSize >= 0 ? originalFileSize : payload.size();
        const int skip = raSnesHeaderSkip(size);
        return md5Hex(payload.mid(skip));
    }
    case RaHashMode::N64:
        return md5Hex(normalizeN64Payload(payload, extension));
    default:
        return { };
    }
}

RaHasher::Result RaHasher::compute(const QString &filePath, int remusSystemId, const QString &extension) {
    Result result;
    if (filePath.isEmpty() || remusSystemId <= 0) {
        result.error = QStringLiteral("Missing file path or system id");
        return result;
    }

    const int consoleId = raConsoleId(remusSystemId);
    if (consoleId <= 0) {
        result.error = QStringLiteral("No RetroAchievements console mapping for system id %1").arg(remusSystemId);
        return result;
    }

    const RaHashMode mode = hashModeForSystem(remusSystemId);
    if (mode == RaHashMode::Unsupported) {
        result.error = QStringLiteral("System id %1 has no RA hash rules").arg(remusSystemId);
        return result;
    }

    if (mode == RaHashMode::ExternalOnly) {
        return computeViaExternalTool(filePath, consoleId);
    }

    result = computeNative(filePath, mode, extension);
    if (result.success)
        return result;

    if (!resolvedExternalToolPath().isEmpty()) {
        const Result external = computeViaExternalTool(filePath, consoleId);
        if (external.success)
            return external;
        if (!result.error.isEmpty() && !external.error.isEmpty())
            result.error += QStringLiteral("; ");
        result.error += external.error;
    }

    return result;
}

} // namespace Remus
