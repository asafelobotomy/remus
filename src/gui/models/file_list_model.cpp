#include "file_list_model.h"

#include <QFileInfo>

#include "../controllers/app_controller.h"
#include "../../core/database.h"

namespace Remus {

FileListModel::FileListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return m_items.size();
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return QVariant();
    }

    const FileListItem &item = m_items.at(index.row());
    switch (role) {
    case FileIdRole:
        return item.fileId;
    case GameIdRole:
        return item.gameId;
    case FilenameRole:
        return item.filename;
    case DisplayNameRole:
        return item.displayName;
    case PathRole:
        return item.path;
    case SystemNameRole:
        return item.systemName;
    case StatusRole:
        return item.status;
    case MatchedTitleRole:
        return item.matchedTitle;
    case Crc32Role:
        return item.crc32;
    case Md5Role:
        return item.md5;
    case Sha1Role:
        return item.sha1;
    case FileSizeRole:
        return item.fileSize;
    case ConfidenceRole:
        return item.confidence;
    case MatchedRole:
        return item.matched;
    case ConfirmedRole:
        return item.confirmed;
    case RejectedRole:
        return item.rejected;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FileListModel::roleNames() const
{
    return {
        {FileIdRole, "fileId"},
        {GameIdRole, "gameId"},
        {FilenameRole, "filename"},
        {DisplayNameRole, "displayName"},
        {PathRole, "path"},
        {SystemNameRole, "systemName"},
        {StatusRole, "status"},
        {MatchedTitleRole, "matchedTitle"},
        {Crc32Role, "crc32"},
        {Md5Role, "md5"},
        {Sha1Role, "sha1"},
        {FileSizeRole, "fileSize"},
        {ConfidenceRole, "confidence"},
        {MatchedRole, "matched"},
        {ConfirmedRole, "confirmed"},
        {RejectedRole, "rejected"},
    };
}

void FileListModel::setAppController(AppController *appController)
{
    if (m_appController == appController) {
        return;
    }

    if (m_appController != nullptr) {
        disconnect(m_appController, nullptr, this, nullptr);
    }

    m_appController = appController;
    if (m_appController != nullptr) {
        connect(m_appController, &AppController::libraryOpened, this, &FileListModel::refresh);
        connect(m_appController, &AppController::libraryClosed, this, &FileListModel::clear);
    }

    refresh();
}

void FileListModel::refresh()
{
    beginResetModel();
    m_items.clear();

    if (m_appController != nullptr && m_appController->isLibraryOpen()) {
        Database *db = m_appController->database();
        const QList<FileRecord> files = db->getExistingFiles();
        const QMap<int, Database::MatchResult> matches = db->getAllMatches();

        for (const FileRecord &file : files) {
            FileListItem item;
            item.fileId = file.id;
            item.filename = file.filename;
            item.displayName = !file.baseTitle.isEmpty() ? file.baseTitle : QFileInfo(file.filename).completeBaseName();
            item.path = file.currentPath;
            item.systemName = db->getSystemDisplayName(file.systemId);
            item.status = file.processingStatus;
            item.crc32 = file.crc32;
            item.md5 = file.md5;
            item.sha1 = file.sha1;
            item.fileSize = file.fileSize;

            const auto matchIt = matches.constFind(file.id);
            if (matchIt != matches.constEnd()) {
                item.gameId = matchIt->gameId;
                item.matchedTitle = matchIt->gameTitle;
                item.confidence = matchIt->confidence;
                item.matched = true;
                item.confirmed = matchIt->isConfirmed;
                item.rejected = matchIt->isRejected;
            }

            m_items.append(item);
        }
    }

    endResetModel();
    emit countChanged();
}

void FileListModel::clear()
{
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
}

} // namespace Remus