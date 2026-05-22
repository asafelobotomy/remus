#include "match_controller.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QMetaObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QVector>

#include "app_controller.h"
#include "../models/match_list_model.h"
#include "../../core/constants/confidence.h"
#include "../../core/constants/match_methods.h"
#include "../../core/database.h"
#include "../../core/disc_magic_detector.h"
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
    m_matchAllIndex   = 0;
    m_matchAllCount   = 0;
    m_progressMessage = QStringLiteral("Matching files\u2026");
    emit matchingChanged();
    emit matchProgressChanged();
    emit progressMessageChanged();

    m_matchAllFiles = m_appController->database()->getExistingFiles();
    m_totalMatchFiles = m_matchAllFiles.size();
    emit matchProgressChanged();

    // Process one file per event-loop iteration so the UI stays live and
    // the event loop can deliver network replies between files without
    // needing processEvents().
    QMetaObject::invokeMethod(this, &MatchController::doMatchNext, Qt::QueuedConnection);
}

void MatchController::doMatchNext()
{
    if (m_matchAllIndex >= m_matchAllFiles.size()) {
        const int total = m_matchAllFiles.size();
        m_matching = false;
        m_progressMessage = QStringLiteral("Matched %1 of %2 files.").arg(m_matchAllCount).arg(total);
        emit matchingChanged();
        emit progressMessageChanged();
        setLastMessage(m_progressMessage);
        m_matchAllFiles.clear();
        refreshModel();
        emit libraryChanged();
        return;
    }

    // Guard: library may have been closed between iterations.
    if (m_appController == nullptr || !m_appController->isLibraryOpen()) {
        m_matching = false;
        m_matchAllFiles.clear();
        emit matchingChanged();
        return;
    }

    const FileRecord file = m_matchAllFiles.at(m_matchAllIndex);
    m_progressMessage = QStringLiteral("Matching %1 / %2\u2026")
                            .arg(m_matchAllIndex + 1)
                            .arg(m_matchAllFiles.size());
    emit progressMessageChanged();

    if (matchFileRecord(file))
        m_matchAllCount++;
    m_matchedFiles = ++m_matchAllIndex;
    emit matchProgressChanged();

    QMetaObject::invokeMethod(this, &MatchController::doMatchNext, Qt::QueuedConnection);
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
    QSqlDatabase db = m_appController->database()->database();
    if (!db.transaction()) {
        emit matchError(QStringLiteral("Failed to start transaction."));
        return;
    }
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("UPDATE matches SET is_confirmed = 1 WHERE is_confirmed = 0 AND is_rejected = 0"))) {
        db.rollback();
        emit matchError(QStringLiteral("Failed to confirm matches: %1").arg(q.lastError().text()));
        return;
    }
    // Propagate confirmed game titles to files.base_title so the queue shows the game name.
    if (!q.exec(QStringLiteral(
            "UPDATE files SET base_title = ("
            "  SELECT g.title FROM games g"
            "  JOIN matches m ON m.game_id = g.id"
            "  WHERE m.file_id = files.id AND m.is_confirmed = 1"
            "  LIMIT 1"
            ") WHERE id IN (SELECT file_id FROM matches WHERE is_confirmed = 1)"))) {
        db.rollback();
        emit matchError(QStringLiteral("Failed to propagate titles: %1").arg(q.lastError().text()));
        return;
    }
    if (!db.commit()) {
        db.rollback();
        emit matchError(QStringLiteral("Failed to commit confirmAll transaction."));
        return;
    }
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
        // Detect disc serial to enable serial-based cascade when hash matching fails.
        QString discSerial;
        if (DiscMagicDetector::isDiscImageExtension(file.extension)) {
            DiscHeaderInfo discInfo;
            if (file.isCompressed && !file.archivePath.isEmpty()) {
                const QString memberPath = file.archiveInternalPath.isEmpty()
                    ? file.filename : file.archiveInternalPath;
                discInfo = DiscMagicDetector::detectFromArchive(
                    file.archivePath, memberPath, file.fileSize);
            } else {
                discInfo = DiscMagicDetector::detect(file.currentPath);
                if (!discInfo.detected || discInfo.serial.isEmpty()) {
                    const DiscHeaderInfo dcInfo = DiscMagicDetector::extractDreamcastHeader(file.currentPath);
                    if (dcInfo.detected && !dcInfo.serial.isEmpty())
                        discInfo = dcInfo;
                }
            }
            if (discInfo.detected && !discInfo.serial.isEmpty())
                discSerial = discInfo.serial;
        }

        const QString displayName = deriveMatchingDisplayName(file);
        metadata = orchestrator->searchWithFallback(hash, displayName, systemName,
                                                    file.crc32, file.md5, file.sha1, discSerial);
        if (!metadata.title.isEmpty()) {
            if (metadata.matchScore > 0.0f && !metadata.matchMethod.isEmpty()) {
                confidence = metadata.matchScore * 100.0f;
                method = metadata.matchMethod;
            } else {
                confidence = calculateNameSimilarity(displayName, metadata.title);
                method = confidence >= Constants::Confidence::Thresholds::EXACT_NAME
                    ? QString::fromLatin1(Constants::MatchMethods::NAME)
                    : QString::fromLatin1(Constants::MatchMethods::FUZZY);
            }
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

    if (!db->insertMatch(file.id, gameId, confidence, method, confidence / 100.0f)) {
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
    // Stop any in-progress matchAll() pass.
    m_matchAllFiles.clear();
    m_matchAllIndex = 0;
    if (m_matching) {
        m_matching = false;
        emit matchingChanged();
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