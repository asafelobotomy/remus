#include "match_controller.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSqlQuery>
#include <QVector>

#include "app_controller.h"
#include "../models/match_list_model.h"
#include "../../core/constants/confidence.h"
#include "../../core/constants/match_methods.h"
#include "../../core/database.h"
#include "../../core/match_utils.h"
#include "../../core/matching_engine.h"
#include "../../metadata/metadata_provider.h"
#include "../../metadata/provider_orchestrator.h"

namespace Remus {

MatchController::MatchController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
{
    connect(m_appController, &AppController::orchestratorChanged, this, &MatchController::connectOrchestratorSignals);
    connect(m_appController, &AppController::libraryClosed,       this, &MatchController::clearState);
    connect(this, &MatchController::libraryChanged, m_appController, &AppController::refreshSelectedMatch);
    connectOrchestratorSignals();
}

void MatchController::refreshModel()
{
    if (m_model == nullptr) {
        return;
    }

    QList<MatchListEntry> entries;
    if (m_appController != nullptr && m_appController->isLibraryOpen()) {
        Database *db = m_appController->database();
        const QList<FileRecord> files = db->getExistingFiles();
        const QMap<int, Database::MatchResult> matches = db->getAllMatches();
        for (const FileRecord &file : files) {
            const auto matchIt = matches.constFind(file.id);
            if (matchIt == matches.constEnd()) {
                continue;
            }

            MatchListEntry entry;
            entry.fileId = file.id;
            entry.gameId = matchIt->gameId;
            entry.fileName = file.filename;
            entry.title = matchIt->gameTitle;
            entry.system = db->getSystemDisplayName(matchIt->systemId);
            entry.provider = QStringLiteral("matched");
            entry.method = matchIt->matchMethod;
            entry.confidence = matchIt->confidence;
            entry.confirmed = matchIt->isConfirmed;
            entry.rejected = matchIt->isRejected;
            entries.append(entry);
        }
    }

    m_model->setEntries(entries);
    updateUnconfirmedCount();
}

void MatchController::matchSelected()
{
    if (m_matching) {
        emit matchError(QStringLiteral("Matching is already running."));
        return;
    }

    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        emit matchError(QStringLiteral("Open a library before matching files."));
        return;
    }

    const int fileId = m_appController->selectedFileId();
    if (fileId <= 0) {
        emit matchError(QStringLiteral("Select a file first."));
        return;
    }

    m_matching = true;
    m_matchedFiles    = 0;
    m_totalMatchFiles = 1;
    m_progressMessage = QStringLiteral("Matching selected file\u2026");
    emit matchingChanged();
    emit matchProgressChanged();
    emit progressMessageChanged();

    const bool matched = matchFileRecord(m_appController->database()->getFileById(fileId));

    m_matchedFiles = 1;
    m_matching = false;
    m_progressMessage = matched ? QStringLiteral("Match found.") : QStringLiteral("No match found.");
    emit matchingChanged();
    emit matchProgressChanged();
    emit progressMessageChanged();

    if (!matched) {
        emit matchError(QStringLiteral("No metadata match found for the selected file."));
        return;
    }

    refreshModel();
    emit libraryChanged();
}

void MatchController::matchAll()
{
    if (m_matching) {
        emit matchError(QStringLiteral("Matching is already running."));
        return;
    }

    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        emit matchError(QStringLiteral("Open a library before matching files."));
        return;
    }

    m_matching = true;
    m_matchedFiles    = 0;
    m_totalMatchFiles = 0;
    m_progressMessage = QStringLiteral("Matching files\u2026");
    emit matchingChanged();
    emit matchProgressChanged();
    emit progressMessageChanged();

    int matchedCount = 0;
    const QList<FileRecord> files = m_appController->database()->getExistingFiles();
    m_totalMatchFiles = files.size();
    emit matchProgressChanged();

    for (const FileRecord &file : files) {
        m_progressMessage = QStringLiteral("Matching %1 / %2\u2026").arg(m_matchedFiles + 1).arg(m_totalMatchFiles);
        emit progressMessageChanged();
        if (matchFileRecord(file)) {
            matchedCount++;
        }
        m_matchedFiles++;
        emit matchProgressChanged();
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    m_matching = false;
    m_progressMessage = QStringLiteral("Matched %1 of %2 files.").arg(matchedCount).arg(files.size());
    emit matchingChanged();
    emit progressMessageChanged();
    setLastMessage(QStringLiteral("Matched %1 of %2 files.").arg(matchedCount).arg(files.size()));
    refreshModel();
    emit libraryChanged();
}

void MatchController::confirmSelected()
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        return;
    }

    const int fileId = m_appController->selectedFileId();
    if (fileId <= 0) {
        emit matchError(QStringLiteral("Select a file first."));
        return;
    }

    if (!m_appController->database()->confirmMatch(fileId)) {
        emit matchError(QStringLiteral("Failed to confirm the current match."));
        return;
    }

    updateUnconfirmedCount();
    refreshModel();
    emit libraryChanged();
}

void MatchController::updateUnconfirmedCount()
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        m_unconfirmedMatchCount = 0;
        return;
    }
    const int count = m_appController->database()->getUnconfirmedMatchCount();
    if (m_unconfirmedMatchCount != count) {
        m_unconfirmedMatchCount = count;
        emit libraryChanged();
    }
}

void MatchController::confirmAll()
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) return;
    QSqlQuery q(m_appController->database()->database());
    q.exec(QStringLiteral("UPDATE matches SET is_confirmed = 1 WHERE is_confirmed = 0 AND is_rejected = 0"));
    // Propagate confirmed game titles to files.base_title so the queue shows the game name.
    q.exec(QStringLiteral(
        "UPDATE files SET base_title = ("
        "  SELECT g.title FROM games g"
        "  JOIN matches m ON m.game_id = g.id"
        "  WHERE m.file_id = files.id AND m.is_confirmed = 1"
        "  LIMIT 1"
        ") WHERE id IN (SELECT file_id FROM matches WHERE is_confirmed = 1)"));
    updateUnconfirmedCount();
    refreshModel();
    emit libraryChanged();
}

void MatchController::rejectSelected()
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        return;
    }

    const int fileId = m_appController->selectedFileId();
    if (fileId <= 0) {
        emit matchError(QStringLiteral("Select a file first."));
        return;
    }

    if (!m_appController->database()->rejectMatch(fileId)) {
        emit matchError(QStringLiteral("Failed to reject the current match."));
        return;
    }

    refreshModel();
    emit libraryChanged();
}

void MatchController::connectOrchestratorSignals()
{
    if (m_connectedOrchestrator != nullptr) {
        disconnect(m_connectedOrchestrator, nullptr, this, nullptr);
    }

    ProviderOrchestrator *orchestrator = m_appController ? m_appController->orchestrator() : nullptr;
    m_connectedOrchestrator = orchestrator;
    if (orchestrator == nullptr) {
        m_currentProvider.clear();
        emit currentProviderChanged();
        return;
    }

    connect(orchestrator, &ProviderOrchestrator::tryingProvider, this, [this](const QString &providerName, const QString &) {
        if (m_currentProvider == providerName) {
            return;
        }
        m_currentProvider = providerName;
        emit currentProviderChanged();
    });

    connect(orchestrator, &ProviderOrchestrator::providerFailed, this, [this](const QString &providerName, const QString &error) {
        setLastMessage(QStringLiteral("%1 failed: %2").arg(providerName, error));
    });
}

bool MatchController::matchFileRecord(const FileRecord &file)
{
    if (m_appController == nullptr) {
        return false;
    }

    ProviderOrchestrator *orchestrator = m_appController->orchestrator();
    Database *db = m_appController->database();
    if (orchestrator == nullptr || db == nullptr || file.id <= 0) {
        return false;
    }

    const QString systemName = db->getSystemDisplayName(file.systemId);
    const QString hash = selectBestMatchHash(file);
    GameMetadata metadata;
    float confidence = 0.0f;
    QString method;

    if (!hash.isEmpty()) {
        metadata = orchestrator->getByHashWithFallback(hash, systemName, file.crc32, file.md5, file.sha1);
        if (!metadata.title.isEmpty()) {
            confidence = 100.0f;
            method = QString::fromLatin1(Constants::MatchMethods::HASH);
        }
    }

    if (metadata.title.isEmpty()) {
        const QString displayName = deriveMatchingDisplayName(file);
        metadata = orchestrator->searchWithFallback(hash, displayName, systemName, file.crc32, file.md5, file.sha1);
        if (!metadata.title.isEmpty()) {
            confidence = calculateNameSimilarity(displayName, metadata.title);
            method = confidence >= Constants::Confidence::Thresholds::EXACT_NAME
                ? QString::fromLatin1(Constants::MatchMethods::NAME)
                : QString::fromLatin1(Constants::MatchMethods::FUZZY);
        }
    }

    if (metadata.title.isEmpty()) {
        return false;
    }

    const QString genres = metadata.genres.join(QStringLiteral(", "));
    const QString players = metadata.players > 0 ? QString::number(metadata.players) : QString();
    const int gameId = db->insertGame(
        metadata.title,
        file.systemId,
        metadata.region,
        metadata.publisher,
        metadata.developer,
        metadata.releaseDate,
        metadata.description,
        genres,
        players,
        metadata.rating);

    if (gameId <= 0) {
        return false;
    }

    if (!db->insertMatch(file.id, gameId, confidence, method, confidence)) {
        return false;
    }

    setLastMessage(QStringLiteral("Matched %1 -> %2").arg(file.filename, metadata.title));
    return true;
}

float MatchController::calculateNameSimilarity(const QString &left, const QString &right) const
{
    const QString normalizedLeft = left.toLower().simplified();
    const QString normalizedRight = right.toLower().simplified();
    if (normalizedLeft == normalizedRight) {
        return 100.0f;
    }
    if (normalizedLeft.contains(normalizedRight) || normalizedRight.contains(normalizedLeft)) {
        return 90.0f;
    }

    const int distance = MatchingEngine::levenshteinDistance(normalizedLeft, normalizedRight);
    const int maxLength = qMax(normalizedLeft.length(), normalizedRight.length());
    if (maxLength == 0) {
        return 100.0f;
    }

    return qMax(0.0f, (1.0f - static_cast<float>(distance) / static_cast<float>(maxLength)) * 100.0f);
}

void MatchController::clearState()
{
    setLastMessage(QStringLiteral(""));
    if (!m_currentProvider.isEmpty()) {
        m_currentProvider.clear();
        emit currentProviderChanged();
    }
}

void MatchController::setLastMessage(const QString &message)
{
    if (m_lastMessage == message) {
        return;
    }

    m_lastMessage = message;
    emit lastMessageChanged();
}

} // namespace Remus