#pragma once

#include <QObject>
#include <QPointer>

namespace Remus {

class AppController;
class MatchListModel;
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

public:
    explicit MatchController(AppController *appController, QObject *parent = nullptr);

    bool isMatching() const { return m_matching; }
    int matchedFiles() const { return m_matchedFiles; }
    int totalMatchFiles() const { return m_totalMatchFiles; }
    QString progressMessage() const { return m_progressMessage; }
    QString currentProvider() const { return m_currentProvider; }
    QString lastMessage() const { return m_lastMessage; }
    int unconfirmedMatchCount() const { return m_unconfirmedMatchCount; }

    void setModel(MatchListModel *model) { m_model = model; }

    Q_INVOKABLE void refreshModel();
    Q_INVOKABLE void matchSelected();
    Q_INVOKABLE void matchAll();
    Q_INVOKABLE void confirmSelected();
    Q_INVOKABLE void confirmAll();
    Q_INVOKABLE void rejectSelected();

signals:
    void matchingChanged();
    void matchProgressChanged();
    void progressMessageChanged();
    void currentProviderChanged();
    void lastMessageChanged();
    void libraryChanged();
    void matchError(const QString &message);

private:
    void connectOrchestratorSignals();
    void clearState();
    bool matchFileRecord(const FileRecord &file);
    float calculateNameSimilarity(const QString &left, const QString &right) const;
    void setLastMessage(const QString &message);
    void updateUnconfirmedCount();

    AppController *m_appController;
    MatchListModel *m_model = nullptr;
    QPointer<ProviderOrchestrator> m_connectedOrchestrator;
    bool m_matching = false;
    int m_matchedFiles = 0;
    int m_totalMatchFiles = 0;
    QString m_progressMessage;
    QString m_currentProvider;
    QString m_lastMessage;
    int m_unconfirmedMatchCount = 0;
};

} // namespace Remus