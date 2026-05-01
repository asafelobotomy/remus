#include "match_list_model.h"

namespace Remus {

MatchListModel::MatchListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MatchListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_entries.size();
}

QVariant MatchListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const MatchListEntry &entry = m_entries.at(index.row());
    switch (role) {
    case FileIdRole:
        return entry.fileId;
    case GameIdRole:
        return entry.gameId;
    case FileNameRole:
        return entry.fileName;
    case TitleRole:
        return entry.title;
    case SystemRole:
        return entry.system;
    case ProviderRole:
        return entry.provider;
    case MethodRole:
        return entry.method;
    case ConfidenceRole:
        return entry.confidence;
    case ConfirmedRole:
        return entry.confirmed;
    case RejectedRole:
        return entry.rejected;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MatchListModel::roleNames() const
{
    return {
        {FileIdRole, "fileId"},
        {GameIdRole, "gameId"},
        {FileNameRole, "fileName"},
        {TitleRole, "title"},
        {SystemRole, "system"},
        {ProviderRole, "provider"},
        {MethodRole, "method"},
        {ConfidenceRole, "confidence"},
        {ConfirmedRole, "confirmed"},
        {RejectedRole, "rejected"},
    };
}

void MatchListModel::setEntries(const QList<MatchListEntry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void MatchListModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

} // namespace Remus