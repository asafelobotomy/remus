#include "mod_list_model.h"

namespace Remus {

ModListModel::ModListModel(QObject *parent)
    : QAbstractListModel(parent) { }

int ModListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return m_entries.size();
}

QVariant ModListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const ModListEntry &entry = m_entries.at(index.row());
    switch (role) {
    case IdRole:
        return entry.id;
    case TitleRole:
        return entry.title;
    case AuthorRole:
        return entry.author;
    case VersionRole:
        return entry.version;
    case TypeRole:
        return entry.type;
    case FormatRole:
        return entry.format;
    case SystemRole:
        return entry.system;
    case DescriptionRole:
        return entry.description;
    case RatingRole:
        return entry.rating;
    case DownloadsRole:
        return entry.downloads;
    case InstalledRole:
        return entry.installed;
    case InstallationIdRole:
        return entry.installationId;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ModListModel::roleNames() const {
    return {
        { IdRole, "modId" },
        { TitleRole, "title" },
        { AuthorRole, "author" },
        { VersionRole, "version" },
        { TypeRole, "type" },
        { FormatRole, "format" },
        { SystemRole, "system" },
        { DescriptionRole, "description" },
        { RatingRole, "rating" },
        { DownloadsRole, "downloads" },
        { InstalledRole, "installed" },
        { InstallationIdRole, "installationId" },
    };
}

void ModListModel::setEntries(const QList<ModListEntry> &entries) {
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void ModListModel::clear() {
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

} // namespace Remus