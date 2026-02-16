#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include "../../core/database.h"

namespace Remus {

/**
 * @brief Controller for exporting library to emulator frontends
 * 
 * Supports export to RetroArch playlists, ES-DE gamelists, and more.
 * Exposed to QML as a context property.
 */
class ExportController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exporting READ isExporting NOTIFY exportingChanged)
    Q_PROPERTY(int exportProgress READ exportProgress NOTIFY exportProgressChanged)
    Q_PROPERTY(int exportTotal READ exportTotal NOTIFY exportTotalChanged)
    Q_PROPERTY(QString lastExportPath READ lastExportPath NOTIFY lastExportPathChanged)

public:
    /**
     * @brief Supported export formats
     */
    enum class ExportFormat {
        RetroArchPlaylist,    // RetroArch .lpl files
        ESGamelist,           // EmulationStation gamelist.xml
        LaunchBoxXML,         // LaunchBox game database XML
        CSVReport,            // CSV spreadsheet
        JSONExport            // JSON data export
    };
    Q_ENUM(ExportFormat)

    explicit ExportController(Database *db, QObject *parent = nullptr);

    bool isExporting() const { return m_exporting; }
    int exportProgress() const { return m_exportProgress; }
    int exportTotal() const { return m_exportTotal; }
    QString lastExportPath() const { return m_lastExportPath; }

    /**
     * @brief Export library to RetroArch playlist format
     * @param outputDir Directory to write .lpl files
     * @param systems Optional filter: systems to export (empty = all)
     * @param includeUnmatched Include files without confident matches
     * @return Number of playlists created
     */
    Q_INVOKABLE int exportToRetroArch(const QString &outputDir, 
                                       const QStringList &systems = {},
                                       bool includeUnmatched = false);

    /**
     * @brief Export library to EmulationStation gamelist.xml format
     * @param romsDir Base ROMs directory (gamelists written per-system subdirs)
     * @param downloadArtwork Whether to download artwork to media folder
     * @return Number of gamelists created
     */
    Q_INVOKABLE int exportToEmulationStation(const QString &romsDir, 
                                              bool downloadArtwork = false);

    /**
     * @brief Export library to LaunchBox game database XML format
     * @param outputDir Directory to write LaunchBox platform XMLs
     * @param downloadImages Whether to download box art
     * @return Number of platform XMLs created
     */
    Q_INVOKABLE int exportToLaunchBox(const QString &outputDir, 
                                       bool downloadImages = false);

    /**
     * @brief Export library to CSV report
     * @param outputPath Path to write CSV file
     * @param systems Optional filter
     * @return True if successful
     */
    Q_INVOKABLE bool exportToCSV(const QString &outputPath, 
                                  const QStringList &systems = {});

    /**
     * @brief Export full library to JSON
     * @param outputPath Path to write JSON file
     * @param includeMetadata Include full metadata from providers
     * @return True if successful
     */
    Q_INVOKABLE bool exportToJSON(const QString &outputPath, 
                                   bool includeMetadata = true);

    /**
     * @brief Get list of available systems in library
     */
    Q_INVOKABLE QVariantList getAvailableSystems();

    /**
     * @brief Get export statistics preview
     * @param systems Systems to include (empty = all)
     * @return Map with game counts per system
     */
    Q_INVOKABLE QVariantMap getExportPreview(const QStringList &systems = {});

    /**
     * @brief Cancel ongoing export
     */
    Q_INVOKABLE void cancelExport();

    /**
     * @brief Get RetroArch thumbnail path structure
     */
    Q_INVOKABLE QString getRetroArchThumbnailPath(const QString &playlistName, 
                                                    const QString &gameTitle,
                                                    const QString &type);

signals:
    void exportingChanged();
    void exportProgressChanged();
    void exportTotalChanged();
    void lastExportPathChanged();

    void exportStarted(const QString &format);
    void exportCompleted(const QString &format, int itemsExported, const QString &path);
    void exportFailed(const QString &format, const QString &error);
    void exportProgress(int current, int total, const QString &currentItem);

private:
    // RetroArch export helpers
    QString createRetroArchPlaylist(const QString &system, 
                                     const QString &outputDir,
                                     bool includeUnmatched);
    QString sanitizePlaylistName(const QString &name) const;
    QString getRetroArchSystemName(const QString &system) const;

    // ES-DE export helpers
    bool createESGamelist(const QString &system, 
                          const QString &romsDir,
                          bool downloadArtwork);
    QString escapeXml(const QString &text) const;

    // LaunchBox export helpers
    bool createLaunchBoxPlatformXML(const QString &system, 
                                     const QString &outputDir,
                                     bool downloadImages);
    QString getLaunchBoxPlatformName(const QString &system) const;
    QString formatLaunchBoxDate(const QString &isoDate) const;

    Database *m_db;
    bool m_exporting = false;
    bool m_cancelRequested = false;
    int m_exportProgress = 0;
    int m_exportTotal = 0;
    QString m_lastExportPath;
};

} // namespace Remus
