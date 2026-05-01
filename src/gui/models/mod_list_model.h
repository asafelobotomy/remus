#pragma once

#include <QAbstractListModel>
#include <QList>

namespace Remus {

struct ModListEntry {
    QString id;
    QString title;
    QString author;
    QString version;
    QString type;
    QString format;
    QString system;
    QString description;
    double rating = 0.0;
    int downloads = 0;
    bool installed = false;
    int installationId = 0;
};

class ModListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        AuthorRole,
        VersionRole,
        TypeRole,
        FormatRole,
        SystemRole,
        DescriptionRole,
        RatingRole,
        DownloadsRole,
        InstalledRole,
        InstallationIdRole,
    };

    explicit ModListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(const QList<ModListEntry> &entries);
    void clear();

private:
    QList<ModListEntry> m_entries;
};

} // namespace Remus