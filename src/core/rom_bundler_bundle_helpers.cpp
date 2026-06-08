#include "rom_bundler_bundle_helpers.h"

namespace Remus {
namespace BundleHelpers {

    bool isDiscManifestPath(const QString &path) {
        const QString suffix = QFileInfo(path).suffix().toLower();
        return suffix == QStringLiteral("cue") || suffix == QStringLiteral("gdi");
    }

    bool isChdConvertiblePath(const QString &path) {
        const QString suffix = QFileInfo(path).suffix().toLower();
        return suffix == QStringLiteral("cue") || suffix == QStringLiteral("iso") || suffix == QStringLiteral("img")
            || suffix == QStringLiteral("gdi");
    }

    bool isRvzConvertiblePath(const QString &path) {
        const QString extension = QStringLiteral(".") + QFileInfo(path).suffix().toLower();
        return Constants::Files::isRvzSourceExtension(extension);
    }

    QStringList collectArchiveEntries(const QString &rootDir) {
        QStringList entries;
        QDir root(rootDir);
        QDirIterator it(rootDir, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            entries << root.relativeFilePath(it.filePath()).replace('\\', '/');
        }
        entries.sort();
        return entries;
    }

    QString dottedSuffix(const QString &path) {
        const QString suffix = QFileInfo(path).suffix();
        return suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix.toLower();
    }

    bool clearDirectoryExcept(const QString &dirPath, const QString &keepPath) {
        QDir root(dirPath);
        const QFileInfoList entries
            = root.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &entry : entries) {
            if (entry.absoluteFilePath() == keepPath) {
                continue;
            }
            if (entry.isDir()) {
                if (!QDir(entry.absoluteFilePath()).removeRecursively()) {
                    return false;
                }
            } else if (!QFile::remove(entry.absoluteFilePath())) {
                return false;
            }
        }
        return true;
    }

    bool ensurePlaceholderFile(const QString &path, QString *error) {
        const QFileInfo info(path);
        if (!QDir().mkpath(info.absolutePath())) {
            if (error)
                *error = QStringLiteral("Failed to create placeholder directory: %1").arg(info.absolutePath());
            return false;
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            if (error)
                *error = QStringLiteral("Failed to create placeholder file: %1").arg(path);
            return false;
        }
        file.close();
        return true;
    }

    bool copyFileIntoDirectory(
        const QString &sourcePath, const QString &destinationDir, QString *error, const QString &targetFileName) {
        if (sourcePath.isEmpty() || !QFile::exists(sourcePath)) {
            if (error)
                *error = QStringLiteral("Referenced disc file not found: %1").arg(sourcePath);
            return false;
        }

        const QString fileName = targetFileName.isEmpty() ? QFileInfo(sourcePath).fileName() : targetFileName;
        const QString destinationPath = QDir(destinationDir).filePath(fileName);
        if (QFile::exists(destinationPath)) {
            return true;
        }

        if (!QFile::copy(sourcePath, destinationPath)) {
            if (error)
                *error = QStringLiteral("Failed to stage disc file: %1").arg(fileName);
            return false;
        }
        return true;
    }

    QStringList getReferencedDiscFiles(const QString &manifestPath) {
        const QString suffix = QFileInfo(manifestPath).suffix().toLower();
        QFile manifestFile(manifestPath);
        if (!manifestFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return { };
        }

        QStringList referencedFiles;
        QTextStream input(&manifestFile);
        if (suffix == QStringLiteral("cue")) {
            static const QRegularExpression cueFilePattern(
                QStringLiteral("^\\s*FILE\\s+\"([^\"]+)\""), QRegularExpression::CaseInsensitiveOption);
            while (!input.atEnd()) {
                const QRegularExpressionMatch match = cueFilePattern.match(input.readLine());
                if (match.hasMatch()) {
                    referencedFiles << match.captured(1);
                }
            }
        } else if (suffix == QStringLiteral("gdi")) {
            static const QRegularExpression gdiFilePattern(
                QStringLiteral("^\\s*\\d+\\s+\\d+\\s+\\d+\\s+\\d+\\s+(.+?)\\s+\\d+\\s*$"));
            while (!input.atEnd()) {
                const QString line = input.readLine().trimmed();
                if (line.isEmpty()) {
                    continue;
                }
                const QRegularExpressionMatch match = gdiFilePattern.match(line);
                if (!match.hasMatch()) {
                    continue;
                }
                QString fileName = match.captured(1).trimmed();
                if (fileName.startsWith('"') && fileName.endsWith('"') && fileName.size() >= 2) {
                    fileName = fileName.mid(1, fileName.size() - 2);
                }
                referencedFiles << fileName;
            }
        }

        referencedFiles.removeDuplicates();
        return referencedFiles;
    }

    bool stageReferencedDiscFiles(const QString &manifestPath, const QString &destinationDir, QString *error) {
        const QFileInfo manifestInfo(manifestPath);
        const QDir manifestDir = manifestInfo.dir();
        const QStringList referencedFiles = getReferencedDiscFiles(manifestPath);
        for (const QString &relativePath : referencedFiles) {
            // Reject absolute paths and directory-traversal references (e.g. ../evil).
            const QString safeRelPath = ArchiveExtractor::normalizeArchiveMemberPath(relativePath);
            if (safeRelPath.isEmpty()) {
                if (error)
                    *error = QStringLiteral("Unsafe disc manifest reference: %1").arg(relativePath);
                return false;
            }
            const QString sourcePath = manifestDir.filePath(safeRelPath);
            if (!copyFileIntoDirectory(sourcePath, destinationDir, error, QFileInfo(safeRelPath).fileName())) {
                return false;
            }
        }
        return true;
    }

    RomBundler::DiscOutputFormat resolveDiscOutputFormat(const FileRecord &file, const Database::MatchResult &match,
        const QString &payloadPath, const RomBundler::BundleConfig &config) {
        const int resolvedSystemId = match.systemId > 0 ? match.systemId : file.systemId;
        if (config.discOutputFormat == RomBundler::DiscOutputFormat::Original) {
            return RomBundler::DiscOutputFormat::Original;
        }
        if (config.discOutputFormat == RomBundler::DiscOutputFormat::Rvz) {
            return ((resolvedSystemId == Constants::Systems::ID_GAMECUBE
                        || resolvedSystemId == Constants::Systems::ID_WII)
                       && isRvzConvertiblePath(payloadPath))
                ? RomBundler::DiscOutputFormat::Rvz
                : RomBundler::DiscOutputFormat::Original;
        }
        // CHD requested: delegate to ConversionPlanner for system-aware routing.
        // ConversionPlanner may redirect to RVZ (GameCube/Wii) or CSO (PSP).
        // If it returns Original (unknown/unrouted system), fall back to CHD when
        // the payload is a convertible disc format.
        ConversionPlanner::Request req;
        req.systemId = resolvedSystemId;
        req.extension = QStringLiteral(".") + QFileInfo(payloadPath).suffix().toLower();
        req.intent = ConversionPlanner::PlanningIntent::AutoProcess;
        req.availableTools.chdmanAvailable = CHDConverter().isChdmanAvailable();
        req.availableTools.dolphinToolAvailable = RVZConverter().isDolphinToolAvailable();
        req.availableTools.maxcsoAvailable = CSOConverter().isMaxcsoAvailable();
        const ConversionPlanner::Plan plan = ConversionPlanner::plan(req);
        switch (plan.action) {
        case ConversionPlanner::PlannedAction::ConvertToRvz:
            return isRvzConvertiblePath(payloadPath) ? RomBundler::DiscOutputFormat::Rvz
                                                     : RomBundler::DiscOutputFormat::Original;
        case ConversionPlanner::PlannedAction::ConvertToCso:
            return RomBundler::DiscOutputFormat::Cso;
        case ConversionPlanner::PlannedAction::ConvertToChd:
            return isChdConvertiblePath(payloadPath) ? RomBundler::DiscOutputFormat::Chd
                                                     : RomBundler::DiscOutputFormat::Original;
        default:
            // ConversionPlanner doesn't know the system (ID 0 or unrouted); honour the
            // explicit CHD request when the payload format supports it.
            if (config.discOutputFormat == RomBundler::DiscOutputFormat::Chd && isChdConvertiblePath(payloadPath)) {
                return RomBundler::DiscOutputFormat::Chd;
            }
            return RomBundler::DiscOutputFormat::Original;
        }
    }

    bool stageBundlePayloadAtRoot(const QString &payloadPath, const QString &bundleRoot,
        const QList<FileRecord> &childFiles, QString *stagedPayloadPath, QString *error) {
        const QString payloadFileName = QFileInfo(payloadPath).fileName();
        if (!copyFileIntoDirectory(payloadPath, bundleRoot, error, payloadFileName)) {
            return false;
        }
        if (stagedPayloadPath) {
            *stagedPayloadPath = QDir(bundleRoot).filePath(payloadFileName);
        }
        if (isDiscManifestPath(payloadPath) && !stageReferencedDiscFiles(payloadPath, bundleRoot, error)) {
            return false;
        }
        for (const FileRecord &child : childFiles) {
            if (child.currentPath.isEmpty() || !QFile::exists(child.currentPath)) {
                continue;
            }
            if (!copyFileIntoDirectory(child.currentPath, bundleRoot, error, QFileInfo(child.filename).fileName())) {
                return false;
            }
        }
        return true;
    }

    QString findCompanionManifestPath(const QString &sourceRoot, const QList<FileRecord> &childFiles) {
        for (const FileRecord &child : childFiles) {
            if (!isDiscManifestPath(child.filename)) {
                continue;
            }
            QString candidatePath;
            if (!child.archiveInternalPath.isEmpty()) {
                const QString normalized = ArchiveExtractor::normalizeArchiveMemberPath(child.archiveInternalPath);
                if (!normalized.isEmpty()) {
                    candidatePath = QDir(sourceRoot).filePath(normalized);
                }
            }
            if (candidatePath.isEmpty() && !child.filename.isEmpty()) {
                candidatePath = QDir(sourceRoot).filePath(QFileInfo(child.filename).fileName());
            }
            if (!candidatePath.isEmpty() && QFile::exists(candidatePath)) {
                return candidatePath;
            }
        }
        return { };
    }

} // namespace BundleHelpers
} // namespace Remus
