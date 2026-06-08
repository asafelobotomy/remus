#include "verification_result_model.h"

namespace Remus {

VerificationResultModel::VerificationResultModel(QObject *parent)
    : QAbstractListModel(parent) { }

int VerificationResultModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return m_entries.size();
}

QVariant VerificationResultModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }

    const VerificationListEntry &entry = m_entries.at(index.row());
    switch (role) {
    case FileIdRole:
        return entry.fileId;
    case FilenameRole:
        return entry.filename;
    case SystemRole:
        return entry.system;
    case StatusRole:
        return entry.status;
    case FileHashRole:
        return entry.fileHash;
    case DatHashRole:
        return entry.datHash;
    case HashTypeRole:
        return entry.hashType;
    case NotesRole:
        return entry.notes;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> VerificationResultModel::roleNames() const {
    return {
        { FileIdRole, "fileId" },
        { FilenameRole, "filename" },
        { SystemRole, "system" },
        { StatusRole, "status" },
        { FileHashRole, "fileHash" },
        { DatHashRole, "datHash" },
        { HashTypeRole, "hashType" },
        { NotesRole, "notes" },
    };
}

void VerificationResultModel::setEntries(const QList<VerificationListEntry> &entries) {
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void VerificationResultModel::clear() {
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

} // namespace Remus