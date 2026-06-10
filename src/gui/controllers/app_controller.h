#pragma once

#include <QObject>
#include <QVariantMap>
#include <memory>

#include "../../core/database.h"

namespace Remus {

class MetadataCache;
class ProviderOrchestrator;

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString libraryPath READ libraryPath NOTIFY libraryPathChanged)
    Q_PROPERTY(bool libraryOpen READ isLibraryOpen NOTIFY libraryOpenChanged)
    Q_PROPERTY(int currentView READ currentView WRITE setCurrentView NOTIFY currentViewChanged)
    Q_PROPERTY(int selectedFileId READ selectedFileId WRITE setSelectedFileId NOTIFY selectedFileChanged)
    Q_PROPERTY(int selectedGameId READ selectedGameId NOTIFY selectedGameChanged)
    Q_PROPERTY(QVariantMap selectedFileData READ selectedFile NOTIFY selectedFileDataChanged)
    Q_PROPERTY(QVariantMap selectedMatchData READ selectedMatch NOTIFY selectedMatchDataChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

public:
    enum View {
        LibraryView = 0,
        SettingsView = 1,
    };
    Q_ENUM(View)

    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    QString libraryPath() const {
        return m_libraryPath;
    }
    bool isLibraryOpen() const {
        return m_libraryOpen;
    }
    int currentView() const {
        return m_currentView;
    }
    int selectedFileId() const {
        return m_selectedFileId;
    }
    int selectedGameId() const {
        return m_selectedGameId;
    }
    QString statusMessage() const {
        return m_statusMessage;
    }

    Database *database() {
        return &m_database;
    }
    const Database *database() const {
        return &m_database;
    }
    ProviderOrchestrator *orchestrator() const {
        return m_orchestrator.get();
    }

    Q_INVOKABLE bool openLibrary(const QString &dbPath);
    Q_INVOKABLE void closeLibrary();
    Q_INVOKABLE bool eraseLibraryDatabase(
        bool eraseFiles = true, bool eraseMatchData = true, bool eraseApiCache = true, bool eraseArtwork = true);
    Q_INVOKABLE QString defaultLibraryPath() const;
    Q_INVOKABLE QVariantMap selectedFile();
    Q_INVOKABLE QVariantMap selectedMatch();
    Q_INVOKABLE QString systemName(int systemId);

public slots:
    void setCurrentView(int view);
    void setSelectedFileId(int fileId);
    void setStatusMessage(const QString &message);
    void refreshSelectedMatch();
    void refreshSelectedFile();

signals:
    void libraryPathChanged();
    void libraryOpenChanged();
    void currentViewChanged();
    void selectedFileChanged();
    void selectedGameChanged();
    void selectedFileDataChanged();
    void selectedMatchDataChanged();
    void statusMessageChanged();
    void libraryOpened();
    void libraryClosed();
    void libraryDatabaseErased();
    void artworkCacheEraseRequested();
    void orchestratorChanged();

private:
    void rebuildOrchestrator();

    Database m_database;
    QString m_libraryPath;
    bool m_libraryOpen = false;
    int m_currentView = LibraryView;
    int m_selectedFileId = 0;
    int m_selectedGameId = 0;
    QString m_statusMessage;
    std::unique_ptr<MetadataCache> m_cache;
    std::unique_ptr<ProviderOrchestrator> m_orchestrator;
};

} // namespace Remus