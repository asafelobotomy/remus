#include "match_controller.h"

#include <QFileInfo>
#include <QVector>

#include "app_controller.h"
#include "../models/match_list_model.h"
#include "../../core/constants/confidence.h"
#include "../../core/constants/match_methods.h"
#include "../../core/database.h"
#include "../../core/match_utils.h"
#include "../../metadata/metadata_provider.h"
#include "../../metadata/provider_orchestrator.h"

namespace Remus {

MatchController::MatchController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
{
    connect(m_appController, &AppController::orchestratorChanged, this, &MatchController::connectOrchestratorSignals);
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
    emit matchingChanged();
    const bool matched = matchFileRecord(m_appController->database()->getFileById(fileId));
    m_matching = false;
    emit matchingChanged();

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
    emit matchingChanged();

    int matchedCount = 0;
    const QList<FileRecord> files = m_appController->database()->getExistingFiles();
    for (const FileRecord &file : files) {
        if (matchFileRecord(file)) {
            matchedCount++;
        }
    }

    m_matching = false;
    emit matchingChanged();
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
    m_appController->setSelectedFileId(file.id);
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

    const int distance = levenshteinDistance(normalizedLeft, normalizedRight);
    const int maxLength = qMax(normalizedLeft.length(), normalizedRight.length());
    if (maxLength == 0) {
        return 100.0f;
    }

    return qMax(0.0f, (1.0f - static_cast<float>(distance) / static_cast<float>(maxLength)) * 100.0f);
}

int MatchController::levenshteinDistance(const QString &left, const QString &right) const
{
    const int leftLength = left.length();
    const int rightLength = right.length();
    QVector<QVector<int>> matrix(leftLength + 1, QVector<int>(rightLength + 1));

    for (int i = 0; i <= leftLength; ++i) {
        matrix[i][0] = i;
    }
    for (int j = 0; j <= rightLength; ++j) {
        matrix[0][j] = j;
    }

    for (int i = 1; i <= leftLength; ++i) {
        for (int j = 1; j <= rightLength; ++j) {
            const int cost = left[i - 1] == right[j - 1] ? 0 : 1;
            matrix[i][j] = qMin(
                matrix[i - 1][j] + 1,
                qMin(matrix[i][j - 1] + 1, matrix[i - 1][j - 1] + cost));
        }
    }

    return matrix[leftLength][rightLength];
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