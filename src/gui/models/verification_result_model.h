#pragma once

#include <QAbstractListModel>
#include <QList>

namespace Remus {

struct VerificationListEntry {
    int fileId = 0;
    QString filename;
    QString system;
    QString status;
    QString fileHash;
    QString datHash;
    QString hashType;
    QString notes;
};

class VerificationResultModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        FileIdRole = Qt::UserRole + 1,
        FilenameRole,
        SystemRole,
        StatusRole,
        FileHashRole,
        DatHashRole,
        HashTypeRole,
        NotesRole,
    };

    explicit VerificationResultModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(const QList<VerificationListEntry> &entries);
    void clear();

private:
    QList<VerificationListEntry> m_entries;
};

} // namespace Remus