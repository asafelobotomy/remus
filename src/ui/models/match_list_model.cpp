#include "match_list_model.h"
#include "../../core/constants/constants.h"
#include <QDebug>

using namespace Remus::Constants;

namespace Remus {

MatchListModel::MatchListModel(Database *db, QObject *parent)
    : QAbstractListModel(parent)
    , m_db(db)
{
    if (m_db) {
        loadMatches();
    }
}

int MatchListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_matches.count();
}

QVariant MatchListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_matches.count())
        return QVariant();
    
    const MatchItem &match = m_matches.at( index.row());
    
    switch (role) {
    case FileIdRole:
        return match.fileId;
    case FilenameRole:
        return match.filename;
    case GameTitleRole:
        return match.metadata.title;
    case SystemRole:
        return match.metadata.system;
    case RegionRole:
        return match.metadata.region;
    case PublisherRole:
        return match.metadata.publisher;
    case DeveloperRole:
        return match.metadata.developer;
    case ReleaseDateRole:
        return match.metadata.releaseDate;
    case ConfidenceRole:
        return match.confidence;
    case MatchMethodRole:
        return match.matchMethod;
    case ProviderRole:
        return match.metadata.providerId;
    case ConfidenceColorRole:
        return QString::fromUtf8(UI::getConfidenceColor(match.confidence * 100.0f));
    case ConfidenceLabelRole:
        return Confidence::getCategoryLabel(
            Confidence::getCategory(match.confidence * 100.0f));
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MatchListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[FileIdRole] = "fileId";
    roles[FilenameRole] = "filename";
    roles[GameTitleRole] = "gameTitle";
    roles[SystemRole] = "system";
    roles[RegionRole] = "region";
    roles[PublisherRole] = "publisher";
    roles[DeveloperRole] = "developer";
    roles[ReleaseDateRole] = "releaseDate";
    roles[ConfidenceRole] = "confidence";
    roles[MatchMethodRole] = "matchMethod";
    roles[ProviderRole] = "provider";
    roles[ConfidenceColorRole] = "confidenceColor";
    roles[ConfidenceLabelRole] = "confidenceLabel";
    return roles;
}

void MatchListModel::setConfidenceFilter(const QString &filter)
{
    if (m_confidenceFilter == filter)
        return;
    
    m_confidenceFilter = filter;
    emit confidenceFilterChanged();
    loadMatches();
}

void MatchListModel::refresh()
{
    loadMatches();
}

void MatchListModel::confirmMatch(int index)
{
    if (index < 0 || index >= m_matches.count()) {
        qWarning() << "Invalid match index:" << index;
        return;
    }
    
    const MatchItem &match = m_matches.at(index);
    
    // Update database with confirmed match
    if (m_db) {
        // Get file record to get system ID
        FileRecord file = m_db->getFileById(match.fileId);
        if (file.id == 0) {
            qWarning() << "File not found:" << match.fileId;
            return;
        }
        
        // Convert metadata fields for database
        QString genresStr = match.metadata.genres.join(", ");
        QString playersStr = match.metadata.players > 0 ? QString::number(match.metadata.players) : QString();
        
        // Insert game metadata first (or get existing game ID) with complete metadata
        int gameId = m_db->insertGame(
            match.metadata.title,
            file.systemId,
            match.metadata.region,
            match.metadata.publisher,
            match.metadata.developer,
            match.metadata.releaseDate,
            match.metadata.description,
            genresStr,
            playersStr,
            match.metadata.rating
        );
        
        if (gameId == 0) {
            qWarning() << "Failed to insert game metadata";
            return;
        }
        
        // Now store the match with 100% confidence (user confirmed)
        bool success = m_db->insertMatch(match.fileId, gameId, 
            Confidence::Thresholds::USER_CONFIRMED, "user_confirmed");
        
        if (success) {
            qDebug() << "Match confirmed for file" << match.fileId << "-> game" << gameId;
            emit matchConfirmed(match.fileId);
            
            // Remove from list
            beginRemoveRows(QModelIndex(), index, index);
            m_matches.removeAt(index);
            endRemoveRows();
            emit countChanged();
        } else {
            qWarning() << "Failed to store confirmed match in database";
        }
    }
}

void MatchListModel::rejectMatch(int index)
{
    if (index < 0 || index >= m_matches.count()) {
        qWarning() << "Invalid match index:" << index;
        return;
    }
    
    const MatchItem &match = m_matches.at(index);
    emit matchRejected(match.fileId);
    
    // Remove from list
    beginRemoveRows(QModelIndex(), index, index);
    m_matches.removeAt(index);
    endRemoveRows();
    emit countChanged();
}

void MatchListModel::setDatabase(Database *db)
{
    m_db = db;
    if (m_db) {
        loadMatches();
    }
}

void MatchListModel::addMatch(const MatchItem &item)
{
    // Filter based on confidence filter using constants
    if (m_confidenceFilter != "all") {
        float confidence = item.confidence * 100.0f;
        if (m_confidenceFilter == "high" && confidence < Confidence::Thresholds::HIGH)
            return;
        if (m_confidenceFilter == "medium" && 
            (confidence < Confidence::Thresholds::MEDIUM || confidence >= Confidence::Thresholds::HIGH))
            return;
        if (m_confidenceFilter == "low" && confidence >= Confidence::Thresholds::MEDIUM)
            return;
    }
    
    beginInsertRows(QModelIndex(), m_matches.count(), m_matches.count());
    m_matches.append(item);
    endInsertRows();
    emit countChanged();
}

void MatchListModel::loadMatches()
{
    if (!m_db) {
        qWarning() << "MatchListModel: No database set";
        return;
    }
    
    beginResetModel();
    m_matches.clear();
    
    // Load unconfirmed matches from database
    // This would need a proper query method in Database
    // For now, we'll populate it when matches are made
    
    endResetModel();
    emit countChanged();
}

} // namespace Remus
