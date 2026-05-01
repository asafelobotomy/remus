#pragma once

#include <QAbstractListModel>
#include <QList>

namespace Remus {

struct MatchListEntry {
    int fileId = 0;
    int gameId = 0;
    QString fileName;
    QString title;
    QString system;
    QString provider;
    QString method;
    float confidence = 0.0f;
    bool confirmed = false;
    bool rejected = false;
};

class MatchListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        FileIdRole = Qt::UserRole + 1,
        GameIdRole,
        FileNameRole,
        TitleRole,
        SystemRole,
        ProviderRole,
        MethodRole,
        ConfidenceRole,
        ConfirmedRole,
        RejectedRole,
    };

    explicit MatchListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(const QList<MatchListEntry> &entries);
    void clear();

private:
    QList<MatchListEntry> m_entries;
};

} // namespace Remus