#include "export_controller.h"
#include "../../core/system_resolver.h"
#include "../../core/constants/confidence.h"
#include "../../core/constants/exports.h"
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
    int systemId = SystemResolver::systemIdByName(system);
    const QString mappedName = Constants::Exports::retroArchPlaylistNameForSystemId(systemId);
    if (!mappedName.isEmpty()) {
        return mappedName;
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
    emit exportStarted(Constants::Exports::DisplayNames::RETROARCH);
    
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
    emit exportCompleted(Constants::Exports::DisplayNames::RETROARCH, playlistsCreated, outputDir);
    
    return playlistsCreated;
}

QString ExportController::createRetroArchPlaylist(const QString &system, 
                                                    const QString &outputDir,
                                                    bool includeUnmatched)
{
    QString playlistName = getRetroArchSystemName(system);
    QString filename = sanitizePlaylistName(playlistName) + Constants::Exports::Files::PLAYLIST_EXTENSION;
    QString outputPath = outputDir + "/" + filename;
    
    // Query games for this system
    int minConfidence = includeUnmatched ? 0 : static_cast<int>(Constants::Confidence::Thresholds::MEDIUM);
    
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
        item["core_path"] = Constants::Exports::RetroArch::CORE_DETECT;
        item["core_name"] = Constants::Exports::RetroArch::CORE_DETECT;
        
        QString crc = query.value("crc32").toString();
        if (!crc.isEmpty()) {
            item["crc32"] = crc.toUpper() + "|crc";
        }
        
        item["db_name"] = playlistName + Constants::Exports::Files::PLAYLIST_EXTENSION;
        
        items.append(item);
    }
    
    if (items.isEmpty()) {
        return QString();
    }
    
    // Build playlist JSON
    QJsonObject playlist;
    playlist["version"] = Constants::Exports::RetroArch::PLAYLIST_VERSION;
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
    const QString typePath = Constants::Exports::retroArchThumbnailDirectory(type);
    
    // Sanitize game title for filename
    QString sanitizedTitle = gameTitle;
    sanitizedTitle.replace(QRegularExpression("[&*/:`<>?\\\\|]"), "_");
    
    return QString("%1/%2/%3.png").arg(playlistName, typePath, sanitizedTitle);
}

} // namespace Remus
