#pragma once

#include <QAbstractListModel>
#include <QList>

namespace Remus {

class AppController;

struct FileListItem {
    int fileId = 0;
    int gameId = 0;
    QString filename;
    QString displayName;
    QString path;
    QString systemName;
    QString status;
    QString matchedTitle;
    QString crc32;
    QString md5;
    QString sha1;
    qint64 fileSize = 0;
    float confidence = 0.0f;
    bool matched = false;
    bool confirmed = false;
    bool rejected = false;
};

class FileListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        FileIdRole = Qt::UserRole + 1,
        GameIdRole,
        FilenameRole,
        DisplayNameRole,
        PathRole,
        SystemNameRole,
        StatusRole,
        MatchedTitleRole,
        Crc32Role,
        Md5Role,
        Sha1Role,
        FileSizeRole,
        ConfidenceRole,
        MatchedRole,
        ConfirmedRole,
        RejectedRole,
    };

    explicit FileListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setAppController(AppController *appController);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clear();

signals:
    void countChanged();

private:
    AppController *m_appController = nullptr;
    QList<FileListItem> m_items;
};

} // namespace Remus