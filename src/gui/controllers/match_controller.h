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
    Q_PROPERTY(QString currentProvider READ currentProvider NOTIFY currentProviderChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)

public:
    explicit MatchController(AppController *appController, QObject *parent = nullptr);

    bool isMatching() const { return m_matching; }
    QString currentProvider() const { return m_currentProvider; }
    QString lastMessage() const { return m_lastMessage; }

    void setModel(MatchListModel *model) { m_model = model; }

    Q_INVOKABLE void refreshModel();
    Q_INVOKABLE void matchSelected();
    Q_INVOKABLE void matchAll();
    Q_INVOKABLE void confirmSelected();
    Q_INVOKABLE void rejectSelected();

signals:
    void matchingChanged();
    void currentProviderChanged();
    void lastMessageChanged();
    void libraryChanged();
    void matchError(const QString &message);

private:
    void connectOrchestratorSignals();
    void clearState();
    bool matchFileRecord(const FileRecord &file);
    float calculateNameSimilarity(const QString &left, const QString &right) const;
    int levenshteinDistance(const QString &left, const QString &right) const;
    void setLastMessage(const QString &message);

    AppController *m_appController;
    MatchListModel *m_model = nullptr;
    QPointer<ProviderOrchestrator> m_connectedOrchestrator;
    bool m_matching = false;
    QString m_currentProvider;
    QString m_lastMessage;
};

} // namespace Remus