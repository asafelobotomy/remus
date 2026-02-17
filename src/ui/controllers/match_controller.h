#pragma once

#include <QObject>
#include "../core/database.h"
#include "../metadata/provider_orchestrator.h"

namespace Remus {

class MatchService;

class MatchController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool matching READ isMatching NOTIFY matchingChanged)
    
        // Friend class for testing private methods
        friend class MatchControllerTestAccess;
    
public:
    explicit MatchController(Database *db, QObject *parent = nullptr);
    ~MatchController() override;
    
    bool isMatching() const { return m_matching; }
    
    Q_INVOKABLE void startMatching();
    Q_INVOKABLE void stopMatching();
    Q_INVOKABLE void matchFile(int fileId);
    Q_INVOKABLE void confirmMatch(int fileId);
    Q_INVOKABLE void rejectMatch(int fileId);
    
signals:
    void matchingChanged();
    void matchFound(int fileId, const QString &title, float confidence);
    void matchingCompleted(int matched, int total);
    void matchConfirmed(int fileId);
    void matchRejected(int fileId);
    void libraryUpdated();
    
private:
    QString getSystemName(int systemId) const;
    float calculateNameSimilarity(const QString &name1, const QString &name2) const;
    int levenshteinDistance(const QString &s1, const QString &s2) const;
    
    Database *m_db;
    ProviderOrchestrator *m_orchestrator;
    MatchService *m_matchService = nullptr;
    bool m_matching = false;
};

} // namespace Remus
