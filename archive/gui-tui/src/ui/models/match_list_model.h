#pragma once

#include <QAbstractListModel>
#include <QList>
#include "../../core/database.h"
#include "../../metadata/metadata_provider.h"

namespace Remus {

/**
 * @brief Qt model for match review UI
 * 
 * Displays potential metadata matches with confidence scores,
 * allowing users to confirm or reject matches.
 */
class MatchListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QString confidenceFilter READ confidenceFilter WRITE setConfidenceFilter NOTIFY confidenceFilterChanged)
    
public:
    enum MatchRole {
        FileIdRole = Qt::UserRole + 1,
        FilenameRole,
        GameTitleRole,
        SystemRole,
        RegionRole,
        PublisherRole,
        DeveloperRole,
        ReleaseDateRole,
        ConfidenceRole,
        MatchMethodRole,
        ProviderRole,
        ConfidenceColorRole,
        ConfidenceLabelRole
    };
    Q_ENUM(MatchRole)
    
    struct MatchItem {
        int fileId;
        QString filename;
        GameMetadata metadata;
        float confidence;
        QString matchMethod;
    };
    
    explicit MatchListModel(Database *db = nullptr, QObject *parent = nullptr);
    
    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    // Property getters/setters
    QString confidenceFilter() const { return m_confidenceFilter; }
    void setConfidenceFilter(const QString &filter);
    
    // Database operations
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void confirmMatch(int index);
    Q_INVOKABLE void rejectMatch(int index);
    
    void setDatabase(Database *db);
    void addMatch(const MatchItem &item);
    
signals:
    void countChanged();
    void confidenceFilterChanged();
    void matchConfirmed(int fileId);
    void matchRejected(int fileId);
    void errorOccurred(const QString &error);
    
private:
    void loadMatches();
    
    Database *m_db = nullptr;
    QList<MatchItem> m_matches;
    QString m_confidenceFilter = "all"; // all, high, medium, low
};

} // namespace Remus
