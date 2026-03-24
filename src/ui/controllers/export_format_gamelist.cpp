#include "export_controller.h"
#include "../../core/system_resolver.h"
#include "../../core/constants/systems.h"
#include "../../core/constants/providers.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSqlQuery>
#include <QSqlError>
#include <QTextStream>
#include <QDebug>
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
