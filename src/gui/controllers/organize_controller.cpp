#include "organize_controller.h"

#include <QSettings>
#include <QSqlQuery>

#include "app_controller.h"
#include "../../core/constants/constants.h"

namespace Remus {

OrganizeController::OrganizeController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
    , m_engine(std::make_unique<OrganizeEngine>(*appController->database(), this))
{
    connect(this, &OrganizeController::libraryChanged,
            m_appController, &AppController::refreshSelectedFile);
    QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
                       QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    m_namingTemplate = settings.value(
        QString::fromLatin1(Constants::Settings::Organize::NAMING_TEMPLATE),
        Constants::Settings::Defaults::NAMING_TEMPLATE).toString();
}

void OrganizeController::previewOrganize(const QString &destinationDir)
{
    runOrganize(destinationDir, true);
}

void OrganizeController::applyOrganize(const QString &destinationDir)
{
    runOrganize(destinationDir, false);
}

void OrganizeController::organizeAll(const QString &destinationDir)
{
    runOrganize(destinationDir, false, true);
}

void OrganizeController::undoLast()
{
    if (m_lastUndoId <= 0 || !m_engine->undoOperation(m_lastUndoId)) {
        setLastError(QStringLiteral("Nothing is available to undo."));
        return;
    }

    emit libraryChanged();
}

void OrganizeController::setNamingTemplate(const QString &value)
{
    if (m_namingTemplate == value) {
        return;
    }

    m_namingTemplate = value;
    QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
                       QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    settings.setValue(QString::fromLatin1(Constants::Settings::Organize::NAMING_TEMPLATE), value);
    emit namingTemplateChanged();
}

QList<int> OrganizeController::targetFileIds() const
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        return {};
    }

    if (m_appController->selectedFileId() > 0) {
        return {m_appController->selectedFileId()};
    }

    return bundledFileIds();
}

QList<int> OrganizeController::bundledFileIds() const
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        return {};
    }

    // Only files that are hashed, have a confirmed match, and have been bundled
    // are eligible for the Organize step.
    QSqlQuery q(m_appController->database()->database());
    q.prepare(QStringLiteral(
        "SELECT DISTINCT f.id FROM files f "
        "JOIN matches m ON m.file_id = f.id "
        "WHERE f.is_bundled = 1 "
        "  AND m.is_confirmed = 1 AND m.is_rejected = 0 "
        "  AND (f.md5 IS NOT NULL AND f.md5 != '')"));
    if (!q.exec())
        return {};

    QList<int> fileIds;
    while (q.next())
        fileIds.append(q.value(0).toInt());
    return fileIds;
}

QMap<int, GameMetadata> OrganizeController::metadataForFiles(const QList<int> &fileIds) const
{
    QMap<int, GameMetadata> metadata;
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        return metadata;
    }

    Database *db = m_appController->database();
    for (int fileId : fileIds) {
        const Database::MatchResult match = db->getMatchForFile(fileId);
        if (match.matchId <= 0) {
            continue;
        }

        GameMetadata item;
        item.title = match.gameTitle;
        item.region = match.region;
        item.publisher = match.publisher;
        item.developer = match.developer;
        item.description = match.description;
        item.releaseDate = match.releaseYear > 0 ? QString::number(match.releaseYear) : QString();
        item.genres = match.genre.split(',', Qt::SkipEmptyParts);
        item.players = match.players.toInt();
        item.rating = match.rating;
        metadata.insert(fileId, item);
    }

    return metadata;
}

void OrganizeController::setLastError(const QString &message)
{
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit lastErrorChanged();
}

void OrganizeController::runOrganize(const QString &destinationDir, bool dryRun, bool allBundled)
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        setLastError(QStringLiteral("Open a library before organizing files."));
        return;
    }

    const QList<int> fileIds = allBundled ? bundledFileIds() : targetFileIds();
    if (fileIds.isEmpty()) {
        setLastError(allBundled
            ? QStringLiteral("No bundled ROMs found. Complete Stage 5 (Bundle & Rename) before organizing.")
            : QStringLiteral("Select a matched file first, or create matches for your library."));
        return;
    }

    const QMap<int, GameMetadata> metadata = metadataForFiles(fileIds);
    if (metadata.isEmpty()) {
        setLastError(QStringLiteral("The selected files do not have metadata matches yet."));
        return;
    }

    QSettings settings(QString::fromLatin1(Constants::SETTINGS_ORGANIZATION),
                       QString::fromLatin1(Constants::SETTINGS_APPLICATION));
    const bool preserveOriginals = settings.value(
        QString::fromLatin1(Constants::Settings::Organize::PRESERVE_ORIGINALS),
        Constants::Settings::Defaults::PRESERVE_ORIGINALS).toBool();

    m_organizing = true;
    m_organizedFiles = 0;
    m_totalOrganizeFiles = fileIds.size();
    m_progressMessage = dryRun
        ? QStringLiteral("Previewing %1 files\u2026").arg(fileIds.size())
        : QStringLiteral("Organizing %1 files\u2026").arg(fileIds.size());
    emit organizingChanged();
    emit organizeProgressChanged();
    emit progressMessageChanged();

    const bool bySystem = settings.value(
        QString::fromLatin1(Constants::Settings::Organize::BY_SYSTEM),
        Constants::Settings::Defaults::ORGANIZE_BY_SYSTEM).toBool();

    m_engine->setTemplate(m_namingTemplate);
    m_engine->setCollisionStrategy(CollisionStrategy::Rename);
    m_engine->setDryRun(dryRun);
    m_engine->setFolderNaming(bySystem
        ? Constants::FolderNaming::Scheme::Default
        : Constants::FolderNaming::Scheme::None);

    const QList<OrganizeResult> results = m_engine->organizeFiles(
        fileIds,
        metadata,
        destinationDir.trimmed(),
        preserveOriginals ? FileOperation::Copy : FileOperation::Move);

    QVariantList preview;
    m_lastUndoId = 0;
    int orgSucceeded = 0;
    int orgFailed = 0;
    for (const OrganizeResult &result : results) {
        QVariantMap item;
        item.insert(QStringLiteral("success"), result.success);
        item.insert(QStringLiteral("oldPath"), result.oldPath);
        item.insert(QStringLiteral("newPath"), result.newPath);
        item.insert(QStringLiteral("error"), result.error);
        preview.append(item);
        m_lastUndoId = qMax(m_lastUndoId, result.undoId);
        if (result.success) ++orgSucceeded; else ++orgFailed;
    }
    m_previewEntries = preview;
    emit previewEntriesChanged();

    m_organizedFiles = orgSucceeded;
    m_organizing = false;
    m_progressMessage = dryRun
        ? QStringLiteral("Preview: %1 / %2 files.").arg(orgSucceeded).arg(fileIds.size())
        : QStringLiteral("Organized %1 / %2 files.").arg(orgSucceeded).arg(fileIds.size());
    emit organizeProgressChanged();
    emit organizingChanged();
    emit progressMessageChanged();

    if (!dryRun) {
        emit libraryChanged();
    }
}

} // namespace Remus