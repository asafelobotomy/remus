#include "export_controller.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSqlError>
#include <QTextStream>
#include <QDebug>
#include <QDateTime>
#include "../../core/constants/constants.h"
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

ExportController::ExportController(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

QVariantList ExportController::getAvailableSystems()
{
    QVariantList result;
    const int minimumConfidence = static_cast<int>(Constants::Confidence::Thresholds::MEDIUM);
    
    QSqlQuery query(m_db->database());
    query.exec(QString(R"(
        SELECT DISTINCT s.name AS system, COUNT(DISTINCT g.id) as game_count
        FROM games g
        JOIN systems s ON g.system_id = s.id
        JOIN matches m ON g.id = m.game_id
        WHERE m.confidence >= %1
        GROUP BY s.name
        ORDER BY s.name
    )").arg(minimumConfidence));
    
    while (query.next()) {
        QVariantMap system;
        system["name"] = query.value("system");
        system["gameCount"] = query.value("game_count");
        result.append(system);
    }
    
    return result;
}

QVariantMap ExportController::getExportPreview(const QStringList &systems)
{
    QVariantMap result;
    const int minimumConfidence = static_cast<int>(Constants::Confidence::Thresholds::MEDIUM);
    
    QString sql = QString(R"(
        SELECT g.system, COUNT(DISTINCT g.id) as game_count,
               COUNT(DISTINCT f.id) as file_count
        FROM games g
        JOIN matches m ON g.id = m.game_id
        JOIN files f ON m.file_id = f.id
        WHERE m.confidence >= %1
    )").arg(minimumConfidence);
    
    if (!systems.isEmpty()) {
        QStringList placeholders;
        for (int i = 0; i < systems.size(); ++i) {
            placeholders.append("?");
        }
        sql += " AND g.system IN (" + placeholders.join(",") + ")";
    }
    
    sql += " GROUP BY g.system";
    
    QSqlQuery query(m_db->database());
    query.prepare(sql);
    
    for (const QString &system : systems) {
        query.addBindValue(system);
    }
    
    int totalGames = 0;
    int totalFiles = 0;
    QVariantList systemStats;
    
    if (query.exec()) {
        while (query.next()) {
            QVariantMap stat;
            stat["system"] = query.value("system");
            stat["games"] = query.value("game_count");
            stat["files"] = query.value("file_count");
            systemStats.append(stat);
            
            totalGames += query.value("game_count").toInt();
            totalFiles += query.value("file_count").toInt();
        }
    }
    
    result["systems"] = systemStats;
    result["totalGames"] = totalGames;
    result["totalFiles"] = totalFiles;
    
    return result;
}

bool ExportController::exportToCSV(const QString &outputPath, 
                                    const QStringList &systems)
{
    m_exporting = true;
    emit exportingChanged();
    emit exportStarted(Constants::Exports::DisplayNames::CSV);
    const int minimumConfidence = static_cast<int>(Constants::Confidence::Thresholds::MEDIUM);
    
    QString sql = QString(R"(
        SELECT g.title, s.name AS system, g.region, g.release_date AS year,
               g.publisher, g.developer,
               g.genres AS genre, f.filename, f.current_path AS filepath,
               f.crc32, f.md5, f.sha1,
               m.confidence, m.match_method AS match_type
        FROM games g
        JOIN systems s ON g.system_id = s.id
        JOIN matches m ON g.id = m.game_id
        JOIN files f ON m.file_id = f.id
        WHERE m.confidence >= %1
    )").arg(minimumConfidence);
    
    if (!systems.isEmpty()) {
        QStringList placeholders;
        for (int i = 0; i < systems.size(); ++i) {
            placeholders.append("?");
        }
        sql += " AND s.name IN (" + placeholders.join(",") + ")";
    }
    
    sql += " ORDER BY s.name, g.title";
    
    QSqlQuery query(m_db->database());
    query.prepare(sql);
    
    for (const QString &system : systems) {
        query.addBindValue(system);
    }
    
    if (!query.exec()) {
        m_exporting = false;
        emit exportingChanged();
        emit exportFailed(Constants::Exports::DisplayNames::CSV, query.lastError().text());
        return false;
    }
    
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_exporting = false;
        emit exportingChanged();
        emit exportFailed(Constants::Exports::DisplayNames::CSV, "Failed to create file: " + outputPath);
        return false;
    }
    
    QTextStream out(&file);
    
    // Header
    out << "Title,System,Region,Year,Publisher,Developer,Genre,Filename,Path,CRC32,MD5,SHA1,Confidence,MatchType\n";
    
    int rowCount = 0;
    while (query.next()) {
        QStringList fields;
        for (int i = 0; i < 14; ++i) {
            QString value = query.value(i).toString();
            // Escape quotes and wrap in quotes if contains comma
            value.replace("\"", "\"\"");
            if (value.contains(",") || value.contains("\"") || value.contains("\n")) {
                value = "\"" + value + "\"";
            }
            fields.append(value);
        }
        out << fields.join(",") << "\n";
        rowCount++;
    }
    
    file.close();
    
    m_exporting = false;
    m_lastExportPath = outputPath;
    emit exportingChanged();
    emit lastExportPathChanged();
    emit exportCompleted(Constants::Exports::DisplayNames::CSV, rowCount, outputPath);
    
    return true;
}

bool ExportController::exportToJSON(const QString &outputPath, 
                                     bool includeMetadata)
{
    m_exporting = true;
    emit exportingChanged();
    emit exportStarted(Constants::Exports::DisplayNames::JSON);
    const int minimumConfidence = static_cast<int>(Constants::Confidence::Thresholds::MEDIUM);
    
    QSqlQuery query(m_db->database());
    query.exec(QString(R"(
        SELECT g.id, g.title, s.name AS system, g.region, g.release_date AS year, g.publisher, 
               g.developer, g.genres AS genre, g.description, g.players
        FROM games g
        JOIN systems s ON g.system_id = s.id
        JOIN matches m ON g.id = m.game_id
        WHERE m.confidence >= %1
        GROUP BY g.id
        ORDER BY s.name, g.title
    )").arg(minimumConfidence));
    
    QJsonArray games;
    
    while (query.next()) {
        QJsonObject game;
        game["id"] = query.value("id").toInt();
        game["title"] = query.value("title").toString();
        game["system"] = query.value("system").toString();
        game["region"] = query.value("region").toString();
        game["year"] = query.value("year").toString();
        game["publisher"] = query.value("publisher").toString();
        game["developer"] = query.value("developer").toString();
        game["genre"] = query.value("genre").toString();
        game["description"] = query.value("description").toString();
        game["players"] = query.value("players").toString();
        
        // Get associated files
        int gameId = query.value("id").toInt();
        QSqlQuery fileQuery(m_db->database());
        fileQuery.prepare(R"(
            SELECT f.filename, f.current_path AS filepath, f.crc32, f.md5, f.sha1, m.confidence
            FROM files f
            JOIN matches m ON f.id = m.file_id
            WHERE m.game_id = ?
        )");
        fileQuery.addBindValue(gameId);
        
        QJsonArray files;
        if (fileQuery.exec()) {
            while (fileQuery.next()) {
                QJsonObject file;
                file["filename"] = fileQuery.value("filename").toString();
                file["path"] = fileQuery.value("filepath").toString();
                file["crc32"] = fileQuery.value("crc32").toString();
                file["md5"] = fileQuery.value("md5").toString();
                file["sha1"] = fileQuery.value("sha1").toString();
                file["confidence"] = fileQuery.value("confidence").toInt();
                files.append(file);
            }
        }
        game["files"] = files;
        
        // Optionally include metadata sources
        if (includeMetadata) {
            QSqlQuery metaQuery(m_db->database());
            metaQuery.prepare("SELECT provider_name, provider_id, raw_data FROM metadata_sources WHERE game_id = ?");
            metaQuery.addBindValue(gameId);
            
            QJsonArray metadata;
            if (metaQuery.exec()) {
                while (metaQuery.next()) {
                    QJsonObject source;
                    source["provider"] = metaQuery.value("provider_name").toString();
                    source["providerId"] = metaQuery.value("provider_id").toString();
                    
                    QString rawData = metaQuery.value("raw_data").toString();
                    if (!rawData.isEmpty()) {
                        QJsonDocument doc = QJsonDocument::fromJson(rawData.toUtf8());
                        if (doc.isObject()) {
                            source["data"] = doc.object();
                        }
                    }
                    metadata.append(source);
                }
            }
            game["metadataSources"] = metadata;
        }
        
        games.append(game);
    }
    
    // Build final document
    QJsonObject root;
    root["version"] = QStringLiteral("1.0");
    root["exportDate"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["gameCount"] = games.size();
    root["games"] = games;
    
    // Write to file
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_exporting = false;
        emit exportingChanged();
        emit exportFailed(Constants::Exports::DisplayNames::JSON, "Failed to create file: " + outputPath);
        return false;
    }
    
    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    m_exporting = false;
    m_lastExportPath = outputPath;
    emit exportingChanged();
    emit lastExportPathChanged();
    emit exportCompleted(Constants::Exports::DisplayNames::JSON, games.size(), outputPath);
    
    return true;
}

void ExportController::cancelExport()
{
    m_cancelRequested = true;
}

} // namespace Remus
