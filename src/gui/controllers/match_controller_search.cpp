#include "match_controller.h"

#include "app_controller.h"
#include "../../core/constants/constants.h"
#include "../../core/constants/match_methods.h"
#include "../../core/database.h"
#include "../../core/match_utils.h"
#include "../../metadata/metadata_provider.h"
#include "../../metadata/provider_orchestrator.h"

#include <QSqlQuery>
#include <algorithm>

namespace Remus {

namespace {

    QString releaseYearFromDate(const QString &releaseDate) {
        if (releaseDate.isEmpty())
            return QString();
        const int year = releaseDate.left(4).toInt();
        return year > 0 ? QString::number(year) : QString();
    }

} // namespace

QStringList MatchController::enabledProviders() const {
    if (m_appController == nullptr || m_appController->orchestrator() == nullptr)
        return { };
    return m_appController->orchestrator()->getEnabledProviders();
}

void MatchController::setSearchQuery(const QString &query) {
    if (m_searchQuery == query)
        return;
    m_searchQuery = query;
    emit searchQueryChanged();
}

void MatchController::setSearchProvider(const QString &provider) {
    if (m_searchProvider == provider)
        return;
    m_searchProvider = provider;
    emit searchProviderChanged();
}

void MatchController::clearSearchState() {
    m_searchFileId = 0;
    m_searchRomPath.clear();
    m_searchSystem.clear();
    m_searchQuery.clear();
    m_searchProvider.clear();
    m_searchResults.clear();
    m_searchResultObjects.clear();
    m_previewMetadata.clear();
    m_selectedMetadata = GameMetadata();
    m_selectedSearchIndex = -1;
    m_searching = false;
    m_searchStatus.clear();
    emit searchContextChanged();
    emit searchQueryChanged();
    emit searchProviderChanged();
    emit searchResultsChanged();
    emit previewMetadataChanged();
    emit selectedSearchIndexChanged();
    emit searchingChanged();
    emit searchStatusChanged();
}

void MatchController::setSearchStatus(const QString &status) {
    if (m_searchStatus == status)
        return;
    m_searchStatus = status;
    emit searchStatusChanged();
}

QVariantMap MatchController::metadataToVariantMap(const GameMetadata &metadata) const {
    QVariantMap map;
    map.insert(QStringLiteral("id"), metadata.id);
    map.insert(QStringLiteral("title"), metadata.title);
    map.insert(QStringLiteral("system"), metadata.system);
    map.insert(QStringLiteral("region"), metadata.region);
    map.insert(QStringLiteral("publisher"), metadata.publisher);
    map.insert(QStringLiteral("developer"), metadata.developer);
    map.insert(QStringLiteral("releaseDate"), metadata.releaseDate);
    map.insert(QStringLiteral("releaseYear"), releaseYearFromDate(metadata.releaseDate));
    map.insert(QStringLiteral("description"), metadata.description);
    map.insert(QStringLiteral("genre"), metadata.genres.join(QStringLiteral(", ")));
    map.insert(QStringLiteral("rating"), metadata.rating);
    map.insert(QStringLiteral("provider"), metadata.providerId);
    map.insert(QStringLiteral("method"), metadata.matchMethod);
    map.insert(QStringLiteral("confidence"), metadata.matchScore > 0.0f ? metadata.matchScore * 100.0f : 0.0f);
    map.insert(QStringLiteral("boxArtUrl"), metadata.boxArtUrl);
    return map;
}

QVariantMap MatchController::searchResultToVariantMap(const SearchResult &result, int index) const {
    QVariantMap map;
    map.insert(QStringLiteral("index"), index);
    map.insert(QStringLiteral("id"), result.id);
    map.insert(QStringLiteral("title"), result.title);
    map.insert(QStringLiteral("system"), result.system);
    map.insert(QStringLiteral("region"), result.region);
    map.insert(QStringLiteral("releaseYear"), result.releaseYear);
    map.insert(QStringLiteral("provider"), result.provider);
    map.insert(QStringLiteral("confidence"), result.matchScore > 0.0f ? qRound(result.matchScore * 100.0f) : 0);
    return map;
}

void MatchController::beginSearch(int fileId) {
    clearSearchState();

    if (m_appController == nullptr || !m_appController->isLibraryOpen() || fileId <= 0) {
        emit matchError(QStringLiteral("Select a ROM before searching for a match."));
        return;
    }

    const FileRecord file = m_appController->database()->getFileById(fileId);
    if (file.id <= 0) {
        emit matchError(QStringLiteral("Selected ROM was not found in the library."));
        return;
    }

    m_searchFileId = fileId;
    m_searchRomPath = file.currentPath;
    m_searchSystem = m_appController->database()->getSystemDisplayName(file.systemId);
    m_searchQuery = deriveMatchingDisplayName(file);

    const QStringList providers = enabledProviders();
    if (providers.contains(QStringLiteral("compendium")))
        m_searchProvider = QStringLiteral("compendium");
    else if (!providers.isEmpty())
        m_searchProvider = providers.first();

    emit searchContextChanged();
    emit searchQueryChanged();
    emit searchProviderChanged();

    runSearch(m_searchProvider, m_searchQuery);
}

void MatchController::runSearch(const QString &provider, const QString &query) {
    if (m_searchFileId <= 0 || m_appController == nullptr || !m_appController->isLibraryOpen())
        return;

    ProviderOrchestrator *orchestrator = m_appController->orchestrator();
    Database *db = m_appController->database();
    if (orchestrator == nullptr || db == nullptr)
        return;

    const FileRecord file = db->getFileById(m_searchFileId);
    if (file.id <= 0)
        return;

    const QString trimmedQuery = query.trimmed();
    if (trimmedQuery.isEmpty()) {
        setSearchStatus(QStringLiteral("Enter a search query."));
        return;
    }

    setSearchQuery(trimmedQuery);
    setSearchProvider(provider);

    m_searching = true;
    m_searchResults.clear();
    m_searchResultObjects.clear();
    m_previewMetadata.clear();
    m_selectedMetadata = GameMetadata();
    m_selectedSearchIndex = -1;
    emit searchingChanged();
    emit searchResultsChanged();
    emit previewMetadataChanged();
    emit selectedSearchIndexChanged();
    setSearchStatus(QStringLiteral("Searching\u2026"));

    const QString systemName = db->getSystemDisplayName(file.systemId);
    QList<SearchResult> results;

    prependHashCandidates(orchestrator, file, systemName, provider, results);

    const QList<SearchResult> nameResults = orchestrator->searchProvider(provider, trimmedQuery, systemName);
    for (const SearchResult &result : nameResults) {
        const auto duplicate = std::any_of(results.cbegin(), results.cend(), [&](const SearchResult &existing) {
            return existing.provider == result.provider && existing.id == result.id && existing.title == result.title;
        });
        if (!duplicate)
            results.append(result);
    }

    std::sort(results.begin(), results.end(),
        [](const SearchResult &a, const SearchResult &b) { return a.matchScore > b.matchScore; });

    m_searchResultObjects = results;
    m_searchResults.clear();
    for (int i = 0; i < results.size(); ++i)
        m_searchResults.append(searchResultToVariantMap(results.at(i), i));

    m_searching = false;
    emit searchingChanged();
    emit searchResultsChanged();

    if (results.isEmpty()) {
        setSearchStatus(QStringLiteral("No results found."));
        return;
    }

    setSearchStatus(QStringLiteral("Found %1 result(s).").arg(results.size()));
    selectSearchResult(0);
}

bool MatchController::prependHashCandidates(ProviderOrchestrator *orchestrator, const FileRecord &file,
    const QString &systemName, const QString &providerFilter, QList<SearchResult> &results) const {
    const QString hash = selectBestMatchHash(file);
    if (hash.isEmpty())
        return false;

    GameMetadata hashMetadata;
    if (providerFilter.isEmpty()) {
        hashMetadata = lookupHashWithFallback(orchestrator, m_appController ? m_appController->database() : nullptr,
            file, systemName);
    } else {
        hashMetadata = orchestrator->getHashFromProvider(
            providerFilter, hash, systemName, file.crc32, file.md5, file.sha1, file.raMd5);
    }

    if (hashMetadata.title.isEmpty())
        return false;

    SearchResult hashResult;
    hashResult.id = hashMetadata.id;
    hashResult.title = hashMetadata.title;
    hashResult.system = hashMetadata.system.isEmpty() ? systemName : hashMetadata.system;
    hashResult.region = hashMetadata.region;
    hashResult.releaseYear = releaseYearFromDate(hashMetadata.releaseDate).toInt();
    hashResult.matchScore = hashMetadata.matchScore > 0.0f ? hashMetadata.matchScore : 1.0f;
    hashResult.provider = hashMetadata.providerId.isEmpty() ? providerFilter : hashMetadata.providerId;
    if (hashResult.provider.isEmpty())
        hashResult.provider = QStringLiteral("compendium");

    results.prepend(hashResult);
    return true;
}

void MatchController::selectSearchResult(int index) {
    if (index < 0 || index >= m_searchResultObjects.size()) {
        m_selectedSearchIndex = -1;
        m_selectedMetadata = GameMetadata();
        m_previewMetadata.clear();
        emit selectedSearchIndexChanged();
        emit previewMetadataChanged();
        return;
    }

    if (m_appController == nullptr || m_appController->orchestrator() == nullptr)
        return;

    const SearchResult &result = m_searchResultObjects.at(index);
    m_selectedSearchIndex = index;
    emit selectedSearchIndexChanged();

    GameMetadata metadata
        = m_appController->orchestrator()->fetchProviderMetadata(result.provider, result.id, result.system);
    if (metadata.title.isEmpty()) {
        metadata.title = result.title;
        metadata.system = result.system;
        metadata.region = result.region;
        metadata.providerId = result.provider;
        metadata.matchScore = result.matchScore;
        metadata.matchMethod = result.matchScore >= 1.0f ? QString::fromLatin1(Constants::MatchMethods::HASH)
                                                         : QString::fromLatin1(Constants::MatchMethods::NAME);
        if (result.releaseYear > 0)
            metadata.releaseDate = QString::number(result.releaseYear);
    }

    m_selectedMetadata = metadata;
    m_previewMetadata = metadataToVariantMap(metadata);
    emit previewMetadataChanged();
}

bool MatchController::applyMetadataToDatabase(int fileId, int systemId, const GameMetadata &metadata, float confidence,
    const QString &method, bool skipOverwrite, bool importTitle, bool importDescription, bool importPublisher,
    bool importDeveloper, bool importGenre, bool importRelease, bool importRating) {
    Database *db = m_appController->database();
    if (db == nullptr)
        return false;

    const Database::MatchResult existingMatch = db->getMatchForFile(fileId);
    const bool hasExistingGame = existingMatch.gameId > 0;

    QString title = importTitle ? metadata.title : QString();
    QString region = metadata.region;
    QString publisher = importPublisher ? metadata.publisher : QString();
    QString developer = importDeveloper ? metadata.developer : QString();
    QString releaseDate = importRelease ? metadata.releaseDate : QString();
    QString description = importDescription ? metadata.description : QString();
    QString genres = importGenre ? metadata.genres.join(QStringLiteral(", ")) : QString();
    QString players = metadata.players > 0 ? QString::number(metadata.players) : QString();
    float rating = importRating ? metadata.rating : 0.0f;

    if (skipOverwrite && hasExistingGame) {
        QSqlQuery gameQ(db->database());
        gameQ.prepare(
            QStringLiteral("SELECT title, region, publisher, developer, release_date, description, genres, rating "
                           "FROM games WHERE id = ?"));
        gameQ.addBindValue(existingMatch.gameId);
        if (gameQ.exec() && gameQ.next()) {
            if (!importTitle || title.isEmpty())
                title = gameQ.value(0).toString();
            if (region.isEmpty())
                region = gameQ.value(1).toString();
            if (!importPublisher || publisher.isEmpty())
                publisher = gameQ.value(2).toString();
            if (!importDeveloper || developer.isEmpty())
                developer = gameQ.value(3).toString();
            if (!importRelease || releaseDate.isEmpty())
                releaseDate = gameQ.value(4).toString();
            if (!importDescription || description.isEmpty())
                description = gameQ.value(5).toString();
            if (!importGenre || genres.isEmpty())
                genres = gameQ.value(6).toString();
            if (!importRating || rating <= 0.0f)
                rating = gameQ.value(7).toFloat();
        }
    }

    if (title.isEmpty())
        title = metadata.title;

    int gameId = 0;
    if (hasExistingGame && skipOverwrite) {
        gameId = existingMatch.gameId;
        if (!db->updateGame(
                gameId, publisher, developer, releaseDate, description, genres, players, rating, title, region)) {
            return false;
        }
    } else {
        gameId = db->insertGame(
            title, systemId, region, publisher, developer, releaseDate, description, genres, players, rating);
        if (gameId <= 0)
            return false;
    }

    QSqlQuery clearQ(db->database());
    clearQ.prepare(QStringLiteral("DELETE FROM matches WHERE file_id = ?"));
    clearQ.addBindValue(fileId);
    if (!clearQ.exec())
        return false;

    if (!db->insertMatch(fileId, gameId, confidence, method, confidence / 100.0f))
        return false;

    return true;
}

bool MatchController::applySearchMatch(bool confirmMatch, bool downloadArtwork, bool skipOverwrite, bool importTitle,
    bool importDescription, bool importPublisher, bool importDeveloper, bool importGenre, bool importRelease,
    bool importRating) {
    if (m_searchFileId <= 0 || m_selectedSearchIndex < 0 || m_selectedMetadata.title.isEmpty()) {
        emit matchError(QStringLiteral("Select a search result before applying."));
        return false;
    }

    if (m_appController == nullptr || !m_appController->isLibraryOpen())
        return false;

    const FileRecord file = m_appController->database()->getFileById(m_searchFileId);
    if (file.id <= 0)
        return false;

    const SearchResult &result = m_searchResultObjects.at(m_selectedSearchIndex);
    float confidence = result.matchScore > 0.0f ? result.matchScore * 100.0f : 0.0f;
    QString method = m_selectedMetadata.matchMethod;
    if (method.isEmpty()) {
        method = confidence >= 100.0f ? QString::fromLatin1(Constants::MatchMethods::HASH)
                                      : QString::fromLatin1(Constants::MatchMethods::NAME);
    }

    if (!applyMetadataToDatabase(m_searchFileId, file.systemId, m_selectedMetadata, confidence, method, skipOverwrite,
            importTitle, importDescription, importPublisher, importDeveloper, importGenre, importRelease,
            importRating)) {
        emit matchError(QStringLiteral("Failed to apply the selected match."));
        return false;
    }

    if (confirmMatch && !m_appController->database()->confirmMatch(m_searchFileId)) {
        emit matchError(QStringLiteral("Match saved but confirmation failed."));
        return false;
    }

    setLastMessage(QStringLiteral("Applied match: %1").arg(m_selectedMetadata.title));
    refreshModel();
    emit libraryChanged();
    emit searchMatchApplied();

    if (downloadArtwork) {
        m_appController->setSelectedFileId(m_searchFileId);
        // Artwork download is triggered from QML after dialog closes.
    }

    return true;
}

} // namespace Remus
