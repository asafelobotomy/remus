#include "export_controller.h"
#include "../../core/system_resolver.h"
#include "../../core/constants/systems.h"
#include "../../core/constants/providers.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSqlError>
#include <QTextStream>
#include <QDebug>
#include <QDateTime>
#include <QRegularExpression>
#include <QUrl>
#include "../../metadata/artwork_downloader.h"
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

static QString sanitizeFilename(const QString &name)
{
    QString sanitized = name;
    sanitized.replace(QRegularExpression("[/\\\\:*?\"<>|]"), "_");
    return sanitized;
}

static QUrl extractScreenScraperBoxArt(const QJsonObject &game)
{
    QJsonArray mediaArray;
    if (game.contains("medias")) {
        if (game["medias"].isArray()) {
            mediaArray = game["medias"].toArray();
        } else if (game["medias"].isObject()) {
            QJsonObject mediasObj = game["medias"].toObject();
            if (mediasObj.contains("media") && mediasObj["media"].isArray()) {
                mediaArray = mediasObj["media"].toArray();
            }
        }
    }

    for (const QJsonValue &mediaVal : mediaArray) {
        QJsonObject media = mediaVal.toObject();
        QString type = media["type"].toString().toLower();
        QString url = media["url"].toString();
        if (url.isEmpty()) {
            url = media["url_original"].toString();
        }
        if (url.isEmpty()) {
            url = media["url_thumb"].toString();
        }

        if (!url.isEmpty() && (type.contains("box-2d") || type.contains("box2d") || type == "box")) {
            if (!type.contains("back")) {
                return QUrl(url);
            }
        }
    }

    return QUrl();
}

static QUrl getArtworkUrlForGame(QSqlDatabase &db, int gameId)
{
    QSqlQuery query(db);
    query.prepare(R"(
        SELECT provider_name, raw_data
        FROM metadata_sources
        WHERE game_id = ?
        ORDER BY priority DESC
    )");
    query.addBindValue(gameId);

    if (!query.exec()) {
        return QUrl();
    }

    while (query.next()) {
        QString provider = query.value("provider_name").toString().toLower();
        QString rawData = query.value("raw_data").toString();

        if (rawData.isEmpty()) {
            continue;
        }

        QJsonDocument doc = QJsonDocument::fromJson(rawData.toUtf8());
        if (!doc.isObject()) {
            continue;
        }

        QJsonObject root = doc.object();
        QJsonObject game;

        if (root.contains("response") && root["response"].isObject()) {
            QJsonObject response = root["response"].toObject();
            if (response.contains("jeu") && response["jeu"].isObject()) {
                game = response["jeu"].toObject();
            }
        } else if (root.contains("jeu") && root["jeu"].isObject()) {
            game = root["jeu"].toObject();
        } else {
            game = root;
        }

        if (provider.contains(Constants::Providers::SCREENSCRAPER)) {
            QUrl url = extractScreenScraperBoxArt(game);
            if (url.isValid()) {
                return url;
            }
        }
    }

    return QUrl();
}

ExportController::ExportController(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

QVariantList ExportController::getAvailableSystems()
{
    QVariantList result;
    
    QSqlQuery query(m_db->database());
    query.exec(R"(
        SELECT DISTINCT s.name AS system, COUNT(DISTINCT g.id) as game_count
        FROM games g
        JOIN systems s ON g.system_id = s.id
        JOIN matches m ON g.id = m.game_id
        WHERE m.confidence >= 60
        GROUP BY s.name
        ORDER BY s.name
    )");
    
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
    
    QString sql = R"(
        SELECT g.system, COUNT(DISTINCT g.id) as game_count,
               COUNT(DISTINCT f.id) as file_count
        FROM games g
        JOIN matches m ON g.id = m.game_id
        JOIN files f ON m.file_id = f.id
        WHERE m.confidence >= 60
    )";
    
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

QString ExportController::escapeXml(const QString &text) const
{
    QString escaped = text;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&apos;");
    return escaped;
}

int ExportController::exportToEmulationStation(const QString &romsDir, 
                                                 bool downloadArtwork)
{
    m_exporting = true;
    m_cancelRequested = false;
    emit exportingChanged();
    emit exportStarted("EmulationStation");
    
    // Get all systems
    QStringList systems;
    QSqlQuery query(m_db->database());
    query.exec("SELECT DISTINCT system FROM games ORDER BY system");
    while (query.next()) {
        systems.append(query.value(0).toString());
    }
    
    m_exportTotal = systems.size();
    m_exportProgress = 0;
    emit exportTotalChanged();
    
    int gamelistsCreated = 0;
    
    for (const QString &system : systems) {
        if (m_cancelRequested) break;
        
        if (createESGamelist(system, romsDir, downloadArtwork)) {
            gamelistsCreated++;
        }
        
        m_exportProgress++;
        emit exportProgressChanged();
        emit exportProgress(m_exportProgress, m_exportTotal, system);
    }
    
    m_exporting = false;
    m_lastExportPath = romsDir;
    emit exportingChanged();
    emit lastExportPathChanged();
    emit exportCompleted("EmulationStation", gamelistsCreated, romsDir);
    
    return gamelistsCreated;
}

bool ExportController::createESGamelist(const QString &system, 
                                          const QString &romsDir,
                                          bool downloadArtwork)
{
    // ES-DE uses lowercase system folder names
    QString systemFolder = system.toLower();
    QString systemDir = romsDir + "/" + systemFolder;
    QString gamelistPath = systemDir + "/gamelist.xml";

    ArtworkDownloader downloader;
    QString mediaDir = systemDir + "/media/boxart";
    if (downloadArtwork) {
        QDir().mkpath(mediaDir);
    }
    
    // Query games for this system
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        SELECT g.id, g.title, g.description, g.year, g.developer, g.publisher,
               g.genre, g.players, f.filepath, f.filename
        FROM games g
        JOIN matches m ON g.id = m.game_id
        JOIN files f ON m.file_id = f.id
        WHERE g.system = ? AND m.confidence >= 60
        ORDER BY g.title
    )");
    query.addBindValue(system);
    
    if (!query.exec()) {
        return false;
    }
    
    QString xml = "<?xml version=\"1.0\"?>\n<gameList>\n";
    int gameCount = 0;
    
    while (query.next()) {
        gameCount++;
        
        int gameId = query.value("id").toInt();
        QString filepath = query.value("filepath").toString();
        QString filename = query.value("filename").toString();
        QString title = query.value("title").toString();
        
        // Use relative path from system folder
        QString relativePath = "./" + filename;
        
        xml += "  <game>\n";
        xml += QString("    <path>%1</path>\n").arg(escapeXml(relativePath));
        xml += QString("    <name>%1</name>\n").arg(escapeXml(title));

        QString imageRelativePath;
        if (downloadArtwork) {
            QString imageName = sanitizeFilename(title) + ".png";
            QString imagePath = mediaDir + "/" + imageName;

            if (!QFile::exists(imagePath)) {
                QUrl url = getArtworkUrlForGame(m_db->database(), gameId);
                if (url.isValid()) {
                    downloader.download(url, imagePath);
                }
            }

            if (QFile::exists(imagePath)) {
                imageRelativePath = "./media/boxart/" + imageName;
            }
        }

        if (!imageRelativePath.isEmpty()) {
            xml += QString("    <image>%1</image>\n").arg(escapeXml(imageRelativePath));
        }
        
        QString desc = query.value("description").toString();
        if (!desc.isEmpty()) {
            xml += QString("    <desc>%1</desc>\n").arg(escapeXml(desc));
        }
        
        QString year = query.value("year").toString();
        if (!year.isEmpty()) {
            xml += QString("    <releasedate>%1</releasedate>\n").arg(year + "0101T000000");
        }
        
        QString developer = query.value("developer").toString();
        if (!developer.isEmpty()) {
            xml += QString("    <developer>%1</developer>\n").arg(escapeXml(developer));
        }
        
        QString publisher = query.value("publisher").toString();
        if (!publisher.isEmpty()) {
            xml += QString("    <publisher>%1</publisher>\n").arg(escapeXml(publisher));
        }
        
        QString genre = query.value("genre").toString();
        if (!genre.isEmpty()) {
            xml += QString("    <genre>%1</genre>\n").arg(escapeXml(genre));
        }
        
        QString players = query.value("players").toString();
        if (!players.isEmpty()) {
            xml += QString("    <players>%1</players>\n").arg(escapeXml(players));
        }
        
        xml += "  </game>\n";
    }
    
    xml += "</gameList>\n";
    
    if (gameCount == 0) {
        return false;
    }
    
    // Ensure directory exists
    QDir().mkpath(systemDir);
    
    // Write gamelist.xml
    QFile file(gamelistPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create gamelist:" << gamelistPath;
        return false;
    }
    
    QTextStream out(&file);
    out << xml;
    file.close();
    
    return true;
}

bool ExportController::exportToCSV(const QString &outputPath, 
                                    const QStringList &systems)
{
    m_exporting = true;
    emit exportingChanged();
    emit exportStarted("CSV");
    
    QString sql = R"(
        SELECT g.title, s.name AS system, g.region, g.release_date AS year,
               g.publisher, g.developer,
               g.genres AS genre, f.filename, f.current_path AS filepath,
               f.crc32, f.md5, f.sha1,
               m.confidence, m.match_method AS match_type
        FROM games g
        JOIN systems s ON g.system_id = s.id
        JOIN matches m ON g.id = m.game_id
        JOIN files f ON m.file_id = f.id
        WHERE m.confidence >= 60
    )";
    
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
        emit exportFailed("CSV", query.lastError().text());
        return false;
    }
    
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_exporting = false;
        emit exportingChanged();
        emit exportFailed("CSV", "Failed to create file: " + outputPath);
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
    emit exportCompleted("CSV", rowCount, outputPath);
    
    return true;
}

bool ExportController::exportToJSON(const QString &outputPath, 
                                     bool includeMetadata)
{
    m_exporting = true;
    emit exportingChanged();
    emit exportStarted("JSON");
    
    QSqlQuery query(m_db->database());
    query.exec(R"(
        SELECT g.id, g.title, g.system, g.region, g.year, g.publisher, 
               g.developer, g.genre, g.description, g.players
        FROM games g
        JOIN matches m ON g.id = m.game_id
        WHERE m.confidence >= 60
        GROUP BY g.id
        ORDER BY g.system, g.title
    )");
    
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
            SELECT f.filename, f.filepath, f.crc32, f.md5, f.sha1, m.confidence
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
    root["version"] = "1.0";
    root["exportDate"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["gameCount"] = games.size();
    root["games"] = games;
    
    // Write to file
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_exporting = false;
        emit exportingChanged();
        emit exportFailed("JSON", "Failed to create file: " + outputPath);
        return false;
    }
    
    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    m_exporting = false;
    m_lastExportPath = outputPath;
    emit exportingChanged();
    emit lastExportPathChanged();
    emit exportCompleted("JSON", games.size(), outputPath);
    
    return true;
}

void ExportController::cancelExport()
{
    m_cancelRequested = true;
}

int ExportController::exportToLaunchBox(const QString &outputDir, 
                                         bool downloadImages)
{
    m_exporting = true;
    m_cancelRequested = false;
    emit exportingChanged();
    emit exportStarted("LaunchBox");
    
    QDir dir(outputDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Get all systems
    QStringList systems;
    QSqlQuery query(m_db->database());
    query.exec("SELECT DISTINCT system FROM games ORDER BY system");
    while (query.next()) {
        systems.append(query.value(0).toString());
    }
    
    m_exportTotal = systems.size();
    m_exportProgress = 0;
    emit exportTotalChanged();
    
    int platformsCreated = 0;
    
    for (const QString &system : systems) {
        if (m_cancelRequested) break;
        
        if (createLaunchBoxPlatformXML(system, outputDir, downloadImages)) {
            platformsCreated++;
        }
        
        m_exportProgress++;
        emit exportProgressChanged();
        emit exportProgress(m_exportProgress, m_exportTotal, system);
    }
    
    m_exporting = false;
    m_lastExportPath = outputDir;
    emit exportingChanged();
    emit lastExportPathChanged();
    emit exportCompleted("LaunchBox", platformsCreated, outputDir);
    
    return platformsCreated;
}

bool ExportController::createLaunchBoxPlatformXML(const QString &system, 
                                                    const QString &outputDir,
                                                    bool downloadImages)
{
    QString platformName = getLaunchBoxPlatformName(system);
    QString filename = sanitizeFilename(platformName) + ".xml";
    QString outputPath = outputDir + "/" + filename;
    
    QString imagesDir = outputDir + "/Images/" + platformName;
    if (downloadImages) {
        QDir().mkpath(imagesDir);
    }
    
    // Query games for this system
    QSqlQuery query(m_db->database());
    query.prepare(R"(
        SELECT g.id, g.title, g.description, g.year, g.developer, g.publisher,
               g.genre, g.players, g.rating, f.filepath, f.filename
        FROM games g
        JOIN matches m ON g.id = m.game_id
        JOIN files f ON m.file_id = f.id
        WHERE g.system = ? AND m.confidence >= 60
        ORDER BY g.title
    )");
    query.addBindValue(system);
    
    if (!query.exec()) {
        return false;
    }
    
    // LaunchBox format (simplified database export)
    QString xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    xml += "<LaunchBox>\n";
    int gameCount = 0;
    
    while (query.next()) {
        gameCount++;
        
        int gameId = query.value("id").toInt();
        QString filepath = query.value("filepath").toString();
        QString filename = query.value("filename").toString();
        QString title = query.value("title").toString();
        QString description = query.value("description").toString();
        QString year = query.value("year").toString();
        QString developer = query.value("developer").toString();
        QString publisher = query.value("publisher").toString();
        QString genre = query.value("genre").toString();
        QString players = query.value("players").toString();
        double rating = query.value("rating").toDouble();
        
        xml += "  <Game>\n";
        xml += QString("    <Title>%1</Title>\n").arg(escapeXml(title));
        xml += QString("    <FilePath>.\\%1</FilePath>\n").arg(escapeXml(filename));
        
        if (!description.isEmpty()) {
            xml += QString("    <Description>%1</Description>\n").arg(escapeXml(description));
        }
        
        if (!year.isEmpty()) {
            xml += QString("    <ReleaseYear>%1</ReleaseYear>\n").arg(year);
        }
        
        if (!developer.isEmpty()) {
            xml += QString("    <Developer>%1</Developer>\n").arg(escapeXml(developer));
        }
        
        if (!publisher.isEmpty()) {
            xml += QString("    <Publisher>%1</Publisher>\n").arg(escapeXml(publisher));
        }
        
        if (!genre.isEmpty()) {
            xml += QString("    <Genre>%1</Genre>\n").arg(escapeXml(genre));
        }
        
        if (!players.isEmpty()) {
            xml += QString("    <MaxPlayers>%1</MaxPlayers>\n").arg(players);
        }
        
        if (rating > 0.0) {
            xml += QString("    <Rating>%1</Rating>\n").arg(rating);
        }
        
        // Try to download artwork
        if (downloadImages) {
            QUrl artworkUrl = getArtworkUrlForGame(m_db->database(), gameId);
            if (!artworkUrl.isEmpty()) {
                QString imageName = sanitizeFilename(title) + ".png";
                xml += QString("    <Image>.\\Images\\%1\\%2</Image>\n")
                    .arg(platformName, imageName);
                
                ArtworkDownloader downloader;
                downloader.download(artworkUrl, imagesDir + "/" + imageName);
            }
        }
        
        xml += "  </Game>\n";
    }
    
    xml += "</LaunchBox>\n";
    
    if (gameCount == 0) {
        return false;
    }
    
    // Write platform XML
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create LaunchBox XML:" << outputPath;
        return false;
    }
    
    QTextStream out(&file);
    out << xml;
    file.close();
    
    return true;
}

QString ExportController::getLaunchBoxPlatformName(const QString &system) const
{
    // Map system IDs to LaunchBox platform names
    using namespace Constants::Systems;
    static const QHash<int, QString> platformMap {
        {ID_NES, "Nintendo Entertainment System"},
        {ID_SNES, "Super Nintendo Entertainment System"},
        {ID_N64, "Nintendo 64"},
        {ID_GB, "Nintendo Game Boy"},
        {ID_GBC, "Nintendo Game Boy Color"},
        {ID_GBA, "Nintendo Game Boy Advance"},
        {ID_NDS, "Nintendo DS"},
        {ID_GAMECUBE, "Nintendo GameCube"},
        {ID_WII, "Nintendo Wii"},
        {ID_GENESIS, "Sega Genesis"},
        {ID_MASTER_SYSTEM, "Sega Master System"},
        {ID_GAME_GEAR, "Sega Game Gear"},
        {ID_SATURN, "Sega Saturn"},
        {ID_DREAMCAST, "Sega Dreamcast"},
        {ID_SEGA_CD, "Sega CD"},
        {ID_32X, "Sega 32X"},
        {ID_PSX, "Sony PlayStation"},
        {ID_PS2, "Sony PlayStation 2"},
        {ID_PSP, "Sony PSP"},
        {ID_PSVITA, "Sony PlayStation Vita"},
        {ID_TURBOGRAFX16, "TurboGrafx-16"},
        {ID_TURBOGRAFX_CD, "TurboGrafx-CD"},
        {ID_NEO_GEO, "SNK Neo Geo"},
        {ID_NGP, "SNK Neo Geo Pocket"},
        {ID_ARCADE, "Arcade"},
        {ID_ATARI_2600, "Atari 2600"},
        {ID_ATARI_7800, "Atari 7800"},
        {ID_LYNX, "Atari Lynx"},
        {ID_ATARI_JAGUAR, "Atari Jaguar"},
        {ID_WONDERSWAN, "Bandai WonderSwan"}
    };
    
    // Convert system name to ID, then lookup LaunchBox name
    int systemId = SystemResolver::systemIdByName(system);
    if (systemId > 0 && platformMap.contains(systemId)) {
        return platformMap.value(systemId);
    }
    
    // Fallback: return the input system name
    return system;
}

QString ExportController::formatLaunchBoxDate(const QString &isoDate) const
{
    // Convert YYYY-MM-DD to LaunchBox format (YYYY-MM-DD is same)
    return isoDate;
}

} // namespace Remus
