#include "export_controller.h"

#include <QFile>
#include <QTextStream>

#include "app_controller.h"

namespace Remus {

ExportController::ExportController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
    , m_bundler(std::make_unique<RomBundler>(*appController->database(), this))
{
}

void ExportController::bundleSelected(const QString &destinationDir)
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastMessage(QStringLiteral("Open a library before bundling files."));
        return;
    }

    const int fileId = m_appController->selectedFileId();
    if (fileId <= 0) {
        setLastMessage(QStringLiteral("Select a matched file first."));
        return;
    }

    const FileRecord file = m_appController->database()->getFileById(fileId);
    const Database::MatchResult match = m_appController->database()->getMatchForFile(fileId);
    if (file.id <= 0 || match.matchId <= 0) {
        setLastMessage(QStringLiteral("Bundling requires a selected file with a metadata match."));
        return;
    }

    m_exporting = true;
    emit exportingChanged();

    RomBundler::BundleConfig config;
    const RomBundler::BundleResult result = m_bundler->bundle(file, match, metadataForMatch(match), destinationDir, config);

    m_exporting = false;
    emit exportingChanged();

    if (!result.success) {
        setLastMessage(result.error.isEmpty() ? QStringLiteral("Bundle export failed.") : result.error);
        return;
    }

    m_lastOutputPath = result.outputPath;
    setLastMessage(QStringLiteral("Bundle created: %1").arg(result.outputPath));
    emit exportFinished();
    emit libraryChanged();
}

void ExportController::bundleAll(const QString &destinationDir)
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastMessage(QStringLiteral("Open a library before bundling files."));
        return;
    }

    const auto allMatches = m_appController->database()->getAllMatches();
    if (allMatches.isEmpty()) {
        setLastMessage(QStringLiteral("No matched files to bundle."));
        return;
    }

    m_exporting = true;
    emit exportingChanged();

    int bundled = 0;
    int failed = 0;

    for (auto it = allMatches.constBegin(); it != allMatches.constEnd(); ++it) {
        const FileRecord file = m_appController->database()->getFileById(it.key());
        const Database::MatchResult &match = it.value();
        if (file.id <= 0) {
            continue;
        }

        RomBundler::BundleConfig config;
        const RomBundler::BundleResult result =
            m_bundler->bundle(file, match, metadataForMatch(match), destinationDir, config);

        if (result.success) {
            ++bundled;
        } else {
            ++failed;
        }
    }

    m_exporting = false;
    emit exportingChanged();

    m_lastOutputPath = destinationDir;
    setLastMessage(QStringLiteral("Bundled %1 | Failed %2").arg(bundled).arg(failed));
    if (bundled > 0) {
        emit exportFinished();
        emit libraryChanged();
    }
}

bool ExportController::exportM3u(const QString &outputPath)
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastMessage(QStringLiteral("Open a library before exporting playlists."));
        return false;
    }

    const int fileId = m_appController->selectedFileId();
    if (fileId <= 0) {
        setLastMessage(QStringLiteral("Select a file first."));
        return false;
    }

    const FileRecord file = m_appController->database()->getFileById(fileId);
    QFile outFile(outputPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        setLastMessage(QStringLiteral("Failed to create playlist: %1").arg(outputPath));
        return false;
    }

    QTextStream stream(&outFile);
    stream << file.currentPath << '\n';
    outFile.close();

    m_lastOutputPath = outputPath;
    setLastMessage(QStringLiteral("Playlist exported: %1").arg(outputPath));
    emit exportFinished();
    return true;
}

GameMetadata ExportController::metadataForMatch(const Database::MatchResult &match) const
{
    GameMetadata metadata;
    metadata.title = match.gameTitle;
    metadata.region = match.region;
    metadata.publisher = match.publisher;
    metadata.developer = match.developer;
    metadata.description = match.description;
    metadata.releaseDate = match.releaseYear > 0 ? QString::number(match.releaseYear) : QString();
    metadata.genres = match.genre.split(',', Qt::SkipEmptyParts);
    metadata.players = match.players.toInt();
    metadata.rating = match.rating;
    return metadata;
}

void ExportController::setLastMessage(const QString &message)
{
    if (m_lastMessage == message) {
        return;
    }

    m_lastMessage = message;
    emit lastMessageChanged();
}

} // namespace Remus