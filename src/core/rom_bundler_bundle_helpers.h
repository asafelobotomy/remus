#pragma once

#include "rom_bundler.h"
#include "archive_extractor.h"
#include "chd_converter.h"
#include "conversion_planner.h"
#include "cso_converter.h"
#include "rvz_converter.h"
#include "constants/constants.h"
#include "constants/files.h"
#include "constants/systems.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

namespace Remus {
namespace BundleHelpers {

static constexpr const char *MARKER_FILENAME    = ".remus.md";
static constexpr const char *ARTWORK_SUBDIR     = "artwork";
static constexpr const char *BOXART_FILENAME    = "boxfront.jpg";
static constexpr const char *SCREENSHOTS_SUBDIR = "screenshots";

bool isDiscManifestPath(const QString &path);
bool isChdConvertiblePath(const QString &path);
bool isRvzConvertiblePath(const QString &path);
QStringList collectArchiveEntries(const QString &rootDir);
QString dottedSuffix(const QString &path);
bool clearDirectoryExcept(const QString &dirPath, const QString &keepPath);
bool ensurePlaceholderFile(const QString &path, QString *error);
bool copyFileIntoDirectory(const QString &sourcePath,
                           const QString &destinationDir,
                           QString *error,
                           const QString &targetFileName = QString());
QStringList getReferencedDiscFiles(const QString &manifestPath);
bool stageReferencedDiscFiles(const QString &manifestPath,
                              const QString &destinationDir,
                              QString *error);
RomBundler::DiscOutputFormat resolveDiscOutputFormat(const FileRecord &file,
                                                     const Database::MatchResult &match,
                                                     const QString &payloadPath,
                                                     const RomBundler::BundleConfig &config);
bool stageBundlePayloadAtRoot(const QString &payloadPath,
                              const QString &bundleRoot,
                              const QList<FileRecord> &childFiles,
                              QString *stagedPayloadPath,
                              QString *error);
QString findCompanionManifestPath(const QString &sourceRoot, const QList<FileRecord> &childFiles);

} // namespace BundleHelpers
} // namespace Remus
