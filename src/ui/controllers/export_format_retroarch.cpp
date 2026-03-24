#include "export_controller.h"
#include "../../core/system_resolver.h"
#include "../../core/constants/systems.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSqlError>
#include <QRegularExpression>
#include <QDebug>
#include "../../core/logging_categories.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug() qCDebug(logUi)
#define qInfo() qCInfo(logUi)
#define qWarning() qCWarning(logUi)
#define qCritical() qCCritical(logUi)

namespace Remus {

QString ExportController::getRetroArchSystemName(const QString &system) const
{
    // Map system IDs to RetroArch playlist names (using libretro naming conventions)
    using namespace Constants::Systems;
    static const QHash<int, QString> mapping = {
        {ID_NES, "Nintendo - Nintendo Entertainment System"},
        {ID_SNES, "Nintendo - Super Nintendo Entertainment System"},
        {ID_N64, "Nintendo - Nintendo 64"},
        {ID_GB, "Nintendo - Game Boy"},
        {ID_GBC, "Nintendo - Game Boy Color"},
        {ID_GBA, "Nintendo - Game Boy Advance"},
        {ID_NDS, "Nintendo - Nintendo DS"},
        {ID_GAMECUBE, "Nintendo - GameCube"},
        {ID_WII, "Nintendo - Wii"},
        {ID_GENESIS, "Sega - Mega Drive - Genesis"},
        {ID_MASTER_SYSTEM, "Sega - Master System - Mark III"},
        {ID_GAME_GEAR, "Sega - Game Gear"},
        {ID_SATURN, "Sega - Saturn"},
        {ID_DREAMCAST, "Sega - Dreamcast"},
        {ID_SEGA_CD, "Sega - Mega-CD - Sega CD"},
        {ID_32X, "Sega - 32X"},
        {ID_PSX, "Sony - PlayStation"},
        {ID_PS2, "Sony - PlayStation 2"},
        {ID_PSP, "Sony - PlayStation Portable"},
        {ID_PSVITA, "Sony - PlayStation Vita"},
        {ID_TURBOGRAFX16, "NEC - PC Engine - TurboGrafx 16"},
        {ID_TURBOGRAFX_CD, "NEC - PC Engine CD - TurboGrafx-CD"},
        {ID_NEO_GEO, "SNK - Neo Geo"},
        {ID_NGP, "SNK - Neo Geo Pocket"},
        {ID_ARCADE, "MAME"},
        {ID_ATARI_2600, "Atari - 2600"},
        {ID_ATARI_7800, "Atari - 7800"},
        {ID_LYNX, "Atari - Lynx"},
        {ID_ATARI_JAGUAR, "Atari - Jaguar"},
        {ID_WONDERSWAN, "Bandai - WonderSwan"},
        {ID_VIRTUAL_BOY, "Nintendo - Virtual Boy"}
    };
    
    // Convert system name to ID, then lookup RetroArch name
    int systemId = SystemResolver::systemIdByName(system);
    if (systemId > 0 && mapping.contains(systemId)) {
        return mapping.value(systemId);
    }
    
    // Fallback: return the input system name
    return system;
}

QString ExportController::sanitizePlaylistName(const QString &name) const
{
    QString sanitized = name;
    // Remove characters invalid in filenames
    sanitized.replace(QRegularExpression("[/\\\\:*?\"<>|]"), "_");
    return sanitized;
}

int ExportController::exportToRetroArch(const QString &outputDir, 
                                         const QStringList &systems,
                                         bool includeUnmatched)
{
    m_exporting = true;
    m_cancelRequested = false;
    emit exportingChanged();
    emit exportStarted("RetroArch");
    
    QDir dir(outputDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Get systems to export
    QStringList systemsToExport = systems;
    if (systemsToExport.isEmpty()) {
        QSqlQuery query(m_db->database());
        query.exec("SELECT DISTINCT system FROM games ORDER BY system");
        while (query.next()) {
            systemsToExport.append(query.value(0).toString());
        }
    }
    
    m_exportTotal = systemsToExport.size();
    m_exportProgress = 0;
    emit exportTotalChanged();
    
    int playlistsCreated = 0;
    
    for (const QString &system : systemsToExport) {
        if (m_cancelRequested) break;
        
        QString playlistPath = createRetroArchPlaylist(system, outputDir, includeUnmatched);
        if (!playlistPath.isEmpty()) {
            playlistsCreated++;
        }
        
        m_exportProgress++;
        emit exportProgressChanged();
        emit exportProgress(m_exportProgress, m_exportTotal, system);
    }
    
    m_exporting = false;
    m_lastExportPath = outputDir;
    emit exportingChanged();
    emit lastExportPathChanged();
    emit exportCompleted("RetroArch", playlistsCreated, outputDir);
    
    return playlistsCreated;
}

QString ExportController::createRetroArchPlaylist(const QString &system, 
                                                    const QString &outputDir,
                                                    bool includeUnmatched)
{
    QString playlistName = getRetroArchSystemName(system);
    QString filename = sanitizePlaylistName(playlistName) + ".lpl";
    QString outputPath = outputDir + "/" + filename;
    
    // Query games for this system
    int minConfidence = includeUnmatched ? 0 : 60;
    
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        SELECT g.id, g.title, f.filepath, f.crc32
        FROM games g
        JOIN matches m ON g.id = m.game_id
        JOIN files f ON m.file_id = f.id
        WHERE g.system = ? AND m.confidence >= ?
        ORDER BY g.title
    )");
    query.addBindValue(system);
    query.addBindValue(minConfidence);
    
    if (!query.exec()) {
        qWarning() << "Query failed for system" << system << ":" << query.lastError().text();
        return QString();
    }
    
    QJsonArray items;
    
    while (query.next()) {
        QJsonObject item;
        item["path"] = query.value("filepath").toString();
        item["label"] = query.value("title").toString();
        item["core_path"] = "DETECT";
        item["core_name"] = "DETECT";
        
        QString crc = query.value("crc32").toString();
        if (!crc.isEmpty()) {
            item["crc32"] = crc.toUpper() + "|crc";
        }
        
        item["db_name"] = playlistName + ".lpl";
        
        items.append(item);
    }
    
    if (items.isEmpty()) {
        return QString();
    }
    
    // Build playlist JSON
    QJsonObject playlist;
    playlist["version"] = "1.5";
    playlist["default_core_path"] = "";
    playlist["default_core_name"] = "";
    playlist["label_display_mode"] = 0;
    playlist["right_thumbnail_mode"] = 0;
    playlist["left_thumbnail_mode"] = 0;
    playlist["sort_mode"] = 0;
    playlist["items"] = items;
    
    // Write to file
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create playlist:" << outputPath;
        return QString();
    }
    
    QJsonDocument doc(playlist);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    return outputPath;
}

QString ExportController::getRetroArchThumbnailPath(const QString &playlistName, 
                                                      const QString &gameTitle,
                                                      const QString &type)
{
    QString typePath;
    if (type == "boxart" || type == "cover") {
        typePath = "Named_Boxarts";
    } else if (type == "screenshot" || type == "snap") {
        typePath = "Named_Snaps";
    } else if (type == "title" || type == "titlescreen") {
        typePath = "Named_Titles";
    } else {
        typePath = "Named_Boxarts";
    }
    
    // Sanitize game title for filename
    QString sanitizedTitle = gameTitle;
    sanitizedTitle.replace(QRegularExpression("[&*/:`<>?\\\\|]"), "_");
    
    return QString("%1/%2/%3.png").arg(playlistName, typePath, sanitizedTitle);
}

} // namespace Remus
