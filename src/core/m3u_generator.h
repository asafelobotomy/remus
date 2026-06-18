#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QSet>
#include "database.h"
#include "disc_set_utils.h"

namespace Remus {

/**
 * @brief M3U playlist generator for multi-disc games
 *
 * Groups use persisted `files.disc_set_key` / `files.disc_number` metadata.
 */
class M3UGenerator : public QObject {
    Q_OBJECT

public:
    explicit M3UGenerator(Database &db, QObject *parent = nullptr);

    QMap<QString, QList<int>> detectMultiDiscGames(const QString &systemName = QString());
    QMap<QString, QList<int>> detectMultiDiscGames(const QSet<int> &fileIds);

    bool generateM3U(const QString &gameTitle, const QStringList &discPaths, const QString &outputPath);

    int generateAll(const QString &systemName = QString(), const QString &outputDir = QString());
    int generateAll(const QSet<int> &fileIds, const QString &outputDir = QString());

signals:
    void playlistGenerated(const QString &path, int discCount);
    void errorOccurred(const QString &error);

private:
    Database &m_database;

    QMap<QString, QList<int>> groupByDiscSetKey(const QList<FileRecord> &files) const;
    QList<FileRecord> sortByDiscNumber(const QList<FileRecord> &files, QSqlDatabase *compendiumDb = nullptr) const;
    int catalogDiscNumberForFile(const FileRecord &file, QSqlDatabase *compendiumDb) const;
    QString titleForDiscSet(const QList<FileRecord> &files, QSqlDatabase *compendiumDb = nullptr) const;
    bool writeM3UFile(const QString &path, const QStringList &discPaths);
};

} // namespace Remus
