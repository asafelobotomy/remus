#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>

#include "../../metadata/metadata_provider.h"

namespace Remus {

class AppController;
class Database;
class ProviderOrchestrator;
struct FileRecord;

class MatchController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool matching READ isMatching NOTIFY matchingChanged)
    Q_PROPERTY(int matchedFiles READ matchedFiles NOTIFY matchProgressChanged)
    Q_PROPERTY(int totalMatchFiles READ totalMatchFiles NOTIFY matchProgressChanged)
    Q_PROPERTY(QString progressMessage READ progressMessage NOTIFY progressMessageChanged)
    Q_PROPERTY(QString currentProvider READ currentProvider NOTIFY currentProviderChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(int unconfirmedMatchCount READ unconfirmedMatchCount NOTIFY libraryChanged)

    // Match & enrich dialog (P4)
    Q_PROPERTY(QString searchRomPath READ searchRomPath NOTIFY searchContextChanged)
    Q_PROPERTY(QString searchSystem READ searchSystem NOTIFY searchContextChanged)
    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY searchQueryChanged)
    Q_PROPERTY(QString searchProvider READ searchProvider WRITE setSearchProvider NOTIFY searchProviderChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(QVariantMap previewMetadata READ previewMetadata NOTIFY previewMetadataChanged)
    Q_PROPERTY(int selectedSearchIndex READ selectedSearchIndex NOTIFY selectedSearchIndexChanged)
    Q_PROPERTY(bool searching READ isSearching NOTIFY searchingChanged)
    Q_PROPERTY(QString searchStatus READ searchStatus NOTIFY searchStatusChanged)

public:
    explicit MatchController(AppController *appController, QObject *parent = nullptr);

    bool isMatching() const {
        return m_matching;
    }
    int matchedFiles() const {
        return m_matchedFiles;
    }
    int totalMatchFiles() const {
        return m_totalMatchFiles;
    }
    QString progressMessage() const {
        return m_progressMessage;
    }
    QString currentProvider() const {
        return m_currentProvider;
    }
    QString lastMessage() const {
        return m_lastMessage;
    }
    int unconfirmedMatchCount() const {
        return m_unconfirmedMatchCount;
    }

    QString searchRomPath() const {
        return m_searchRomPath;
    }
    QString searchSystem() const {
        return m_searchSystem;
    }
    QString searchQuery() const {
        return m_searchQuery;
    }
    QString searchProvider() const {
        return m_searchProvider;
    }
    QVariantList searchResults() const {
        return m_searchResults;
    }
    QVariantMap previewMetadata() const {
        return m_previewMetadata;
    }
    int selectedSearchIndex() const {
        return m_selectedSearchIndex;
    }
    bool isSearching() const {
        return m_searching;
    }
    QString searchStatus() const {
        return m_searchStatus;
    }

    Q_INVOKABLE void refreshModel();
    void matchSelected();
    void matchAll();
    Q_INVOKABLE void confirmSelected();
    Q_INVOKABLE void confirmAll();
    Q_INVOKABLE void rejectSelected();

    Q_INVOKABLE QStringList enabledProviders() const;
    Q_INVOKABLE void beginSearch(int fileId);
    Q_INVOKABLE void runSearch(const QString &provider, const QString &query);
    Q_INVOKABLE void selectSearchResult(int index);
    Q_INVOKABLE bool applySearchMatch(bool confirmMatch, bool downloadArtwork, bool skipOverwrite, bool importTitle,
        bool importDescription, bool importPublisher, bool importDeveloper, bool importGenre, bool importRelease,
        bool importRating);

    Q_INVOKABLE void setSearchQuery(const QString &query);
    Q_INVOKABLE void setSearchProvider(const QString &provider);

signals:
    void matchingChanged();
    void matchProgressChanged();
    void progressMessageChanged();
    void currentProviderChanged();
    void lastMessageChanged();
    void libraryChanged();
    void matchError(const QString &message);
    void matchAllFinished();
    void searchContextChanged();
    void searchQueryChanged();
    void searchProviderChanged();
    void searchResultsChanged();
    void previewMetadataChanged();
    void selectedSearchIndexChanged();
    void searchingChanged();
    void searchStatusChanged();
    void searchMatchApplied();

private:
    void connectOrchestratorSignals();
    void clearState();
    bool matchFileRecord(const FileRecord &file);
    float calculateNameSimilarity(const QString &left, const QString &right) const;
    void setLastMessage(const QString &message);
    void updateUnconfirmedCount();
    void doMatchNext();
    void clearSearchState();
    void setSearchStatus(const QString &status);
    QVariantMap metadataToVariantMap(const GameMetadata &metadata) const;
    QVariantMap searchResultToVariantMap(const SearchResult &result, int index) const;
    bool prependHashCandidates(ProviderOrchestrator *orchestrator, const FileRecord &file, const QString &systemName,
        const QString &providerFilter, QList<SearchResult> &results) const;
    GameMetadata lookupHashWithFallback(
        ProviderOrchestrator *orchestrator, Database *db, const FileRecord &file, const QString &systemName) const;
    bool applyMetadataToDatabase(int fileId, int systemId, const GameMetadata &metadata, float confidence,
        const QString &method, bool skipOverwrite, bool importTitle, bool importDescription, bool importPublisher,
        bool importDeveloper, bool importGenre, bool importRelease, bool importRating);

    AppController *m_appController;
    QPointer<ProviderOrchestrator> m_connectedOrchestrator;
    bool m_matching = false;
    int m_matchedFiles = 0;
    int m_totalMatchFiles = 0;
    QString m_progressMessage;
    QString m_currentProvider;
    QString m_lastMessage;
    int m_unconfirmedMatchCount = 0;
    QList<FileRecord> m_matchAllFiles;
    int m_matchAllIndex = 0;
    int m_matchAllCount = 0;

    int m_searchFileId = 0;
    QString m_searchRomPath;
    QString m_searchSystem;
    QString m_searchQuery;
    QString m_searchProvider;
    QVariantList m_searchResults;
    QVariantMap m_previewMetadata;
    QList<SearchResult> m_searchResultObjects;
    GameMetadata m_selectedMetadata;
    int m_selectedSearchIndex = -1;
    bool m_searching = false;
    QString m_searchStatus;
};

} // namespace Remus
