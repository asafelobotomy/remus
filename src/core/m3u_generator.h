#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include "database.h"

namespace Remus {

/**
 * @brief M3U playlist generator for multi-disc games
 * 
 * M3U playlists allow frontends like RetroArch and EmulationStation to treat
 * multi-disc games as single entries with disc swapping support.
 * 
 * Format:
 * ```
 * Final Fantasy VII (USA) (Disc 1).chd
 * Final Fantasy VII (USA) (Disc 2).chd
 * Final Fantasy VII (USA) (Disc 3).chd
 * ```
 * 
 * Saved as: `Final Fantasy VII (USA).m3u`
 */
class M3UGenerator : public QObject {
    Q_OBJECT

public:
    explicit M3UGenerator(Database &db, QObject *parent = nullptr);

    /**
     * @brief Detect multi-disc games in database
     * @param systemName Optional system filter
     * @return Map of game title -> list of file IDs
     */
    QMap<QString, QList<int>> detectMultiDiscGames(const QString &systemName = QString());

    /**
     * @brief Generate M3U playlist for a game
     * @param gameTitle Game title (without disc number)
     * @param discPaths List of disc file paths (in order)
     * @param outputPath Path to write .m3u file
     * @return True if successful
     */
    bool generateM3U(const QString &gameTitle,
                    const QStringList &discPaths,
                    const QString &outputPath);

    /**
     * @brief Generate M3U playlists for all multi-disc games
     * @param systemName Optional system filter
     * @param outputDir Directory to write .m3u files
     * @return Number of playlists created
     */
    int generateAll(const QString &systemName = QString(), 
                   const QString &outputDir = QString());

    /**
     * @brief Check if a game appears to be multi-disc
     * @param filename File name to check
     * @return True if filename contains disc number pattern
     */
    static bool isMultiDisc(const QString &filename);

    /**
     * @brief Extract base game title without disc number
     * @param filename Full filename with disc number
     * @return Base game title
     */
    static QString extractBaseTitle(const QString &filename);

    /**
     * @brief Extract disc number from filename
     * @param filename Filename potentially containing disc number
     * @return Disc number (0 if not found)
     */
    static int extractDiscNumber(const QString &filename);

signals:
    void playlistGenerated(const QString &path, int discCount);
    void errorOccurred(const QString &error);

private:
    Database &m_database;

    /**
     * @brief Group files by base game title
     * @param files List of file records
     * @return Map of base title -> sorted list of files
     */
    QMap<QString, QList<FileRecord>> groupByBaseTitle(const QList<FileRecord> &files);

    /**
     * @brief Sort disc files by disc number
     * @param files List of files for same game
     * @return Sorted list (Disc 1, Disc 2, etc.)
     */
    QList<FileRecord> sortByDiscNumber(const QList<FileRecord> &files);

    /**
     * @brief Write M3U file content
     * @param path Output path
     * @param discPaths List of relative paths to discs
     * @return True if successful
     */
    bool writeM3UFile(const QString &path, const QStringList &discPaths);
};

} // namespace Remus
