#include "match_controller.h"
#include "../../metadata/filename_normalizer.h"
#include "../../services/match_service.h"
#include <QDebug>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QSqlError>
#include <QVector>
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

MatchController::MatchController(Database *db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_matchService(new MatchService())
{
    m_orchestrator = new ProviderOrchestrator(this);
}

MatchController::~MatchController()
{
    delete m_matchService;
}

void MatchController::startMatching()
{
    if (m_matching) {
        qWarning() << "Matching already in progress";
        return;
    }
    
    m_matching = true;
    emit matchingChanged();
    
    // Get all files without matches (or with low confidence matches)
    QList<FileRecord> files = m_db->getAllFiles();
    int matchedCount = 0;
    int totalFiles = files.size();
    
    qDebug() << "Starting metadata matching for" << totalFiles << "files";
    
    for (const FileRecord &file : files) {
        if (!m_matching) {
            qDebug() << "Matching cancelled";
            break;
        }
        
        // Skip if already has high confidence match
        // In real implementation, check matches table first
        
        // Try hash-based matching first (highest confidence)
        QString hash = !file.crc32.isEmpty() ? file.crc32 : 
                       !file.md5.isEmpty() ? file.md5 : file.sha1;
        
        if (!hash.isEmpty()) {
            QString systemName = getSystemName(file.systemId);
            GameMetadata metadata = m_orchestrator->getByHashWithFallback(hash, systemName);
            
            if (!metadata.title.isEmpty()) {
                qDebug() << "Hash match found for" << file.filename << "->" << metadata.title;
                emit matchFound(file.id, metadata.title, 100.0f);
                matchedCount++;
                continue;
            }
        }
        
        // Fall back to name-based matching
        QString cleanName = Metadata::FilenameNormalizer::normalize(file.filename);
        QString systemName = getSystemName(file.systemId);
        
        if (!cleanName.isEmpty()) {
            GameMetadata metadata = m_orchestrator->searchWithFallback("", cleanName, systemName);
            
            if (!metadata.title.isEmpty()) {
                // Calculate confidence based on name similarity
                float confidence = calculateNameSimilarity(cleanName, metadata.title);
                qDebug() << "Name match found for" << file.filename << "->" << metadata.title 
                         << "(" << confidence << "% confidence)";
                emit matchFound(file.id, metadata.title, confidence);
                matchedCount++;
            }
        }
    }
    
    m_matching = false;
    emit matchingChanged();
    emit matchingCompleted(matchedCount, totalFiles);
    
    qDebug() << "Matching complete:" << matchedCount << "/" << totalFiles << "matched";
}

void MatchController::stopMatching()
{
    m_matching = false;
    emit matchingChanged();
    qDebug() << "Matching stopped";
}

void MatchController::matchFile(int fileId)
{
    FileRecord file = m_db->getFileById(fileId);
    if (file.id == 0) {
        qWarning() << "File not found for matching:" << fileId;
        return;
    }
    
    qDebug() << "Matching single file:" << file.filename;
    
    // Try hash-based matching first (highest confidence)
    QString hash = !file.crc32.isEmpty() ? file.crc32 : 
                   !file.md5.isEmpty() ? file.md5 : file.sha1;
    
    if (!hash.isEmpty()) {
        QString systemName = getSystemName(file.systemId);
        GameMetadata metadata = m_orchestrator->getByHashWithFallback(hash, systemName);
        
        if (!metadata.title.isEmpty()) {
            qDebug() << "Hash match found for" << file.filename << "->" << metadata.title;
            
            // Convert genres list to comma-separated string
            QString genresStr = metadata.genres.join(", ");
            QString playersStr = metadata.players > 0 ? QString::number(metadata.players) : QString();
            
            // Insert game and match with complete metadata
            int gameId = m_db->insertGame(metadata.title, file.systemId, metadata.region,
                                          metadata.publisher, metadata.developer, metadata.releaseDate,
                                          metadata.description, genresStr, playersStr, metadata.rating);
            if (gameId > 0) {
                m_db->insertMatch(file.id, gameId, 100.0f, "hash");
            }
            
            emit matchFound(file.id, metadata.title, 100.0f);
            emit libraryUpdated();
            return;
        }
    }
    
    // Fall back to name-based matching
    QString cleanName = Metadata::FilenameNormalizer::normalize(file.filename);
    QString systemName = getSystemName(file.systemId);
    
    if (!cleanName.isEmpty()) {
        GameMetadata metadata = m_orchestrator->searchWithFallback("", cleanName, systemName);
        
        if (!metadata.title.isEmpty()) {
            float confidence = calculateNameSimilarity(cleanName, metadata.title);
            qDebug() << "Name match found for" << file.filename << "->" << metadata.title 
                     << "(" << confidence << "% confidence)";
            
            // Convert genres list to comma-separated string
            QString genresStr = metadata.genres.join(", ");
            QString playersStr = metadata.players > 0 ? QString::number(metadata.players) : QString();
            
            // Insert game and match with complete metadata
            int gameId = m_db->insertGame(metadata.title, file.systemId, metadata.region,
                                          metadata.publisher, metadata.developer, metadata.releaseDate,
                                          metadata.description, genresStr, playersStr, metadata.rating);
            if (gameId > 0) {
                m_db->insertMatch(file.id, gameId, confidence, confidence >= 90 ? "exact" : "fuzzy");
            }
            
            emit matchFound(file.id, metadata.title, confidence);
            emit libraryUpdated();
        }
    }
}

void MatchController::confirmMatch(int fileId)
{
    if (m_matchService->confirmMatch(m_db, fileId)) {
        qDebug() << "Match confirmed for file:" << fileId;
        emit matchConfirmed(fileId);
        emit libraryUpdated();
    } else {
        qWarning() << "Failed to confirm match for file:" << fileId;
    }
}

void MatchController::rejectMatch(int fileId)
{
    if (m_matchService->rejectMatch(m_db, fileId)) {
        qDebug() << "Match rejected for file:" << fileId;
        emit matchRejected(fileId);
        emit libraryUpdated();
    } else {
        qWarning() << "Failed to reject match for file:" << fileId;
    }
}

QString MatchController::getSystemName(int systemId) const
{
    QSqlQuery query(m_db->database());
    query.prepare("SELECT name FROM systems WHERE id = ?");
    query.addBindValue(systemId);
    
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    
    qWarning() << "Failed to find system name for ID:" << systemId;
    return "Unknown";
}

float MatchController::calculateNameSimilarity(const QString &name1, const QString &name2) const
{
    QString n1 = name1.toLower().simplified();
    QString n2 = name2.toLower().simplified();
    
    // Exact match
    if (n1 == n2) return 100.0f;
    
    // If one contains the other completely
    if (n2.contains(n1) || n1.contains(n2)) return 90.0f;
    
    // Calculate Levenshtein distance
    int distance = levenshteinDistance(n1, n2);
    int maxLength = qMax(n1.length(), n2.length());
    
    if (maxLength == 0) return 100.0f;
    
    // Convert to similarity percentage (100% = identical, 0% = completely different)
    float similarity = (1.0f - (float)distance / maxLength) * 100.0f;
    
    // Set minimum threshold
    return qMax(0.0f, similarity);
}

int MatchController::levenshteinDistance(const QString &s1, const QString &s2) const
{
    const int len1 = s1.length();
    const int len2 = s2.length();
    
    // Create distance matrix
    QVector<QVector<int>> d(len1 + 1, QVector<int>(len2 + 1));
    
    // Initialize first column and row
    for (int i = 0; i <= len1; ++i) d[i][0] = i;
    for (int j = 0; j <= len2; ++j) d[0][j] = j;
    
    // Calculate distances
    for (int i = 1; i <= len1; ++i) {
        for (int j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            
            int deletion = d[i - 1][j] + 1;
            int insertion = d[i][j - 1] + 1;
            int substitution = d[i - 1][j - 1] + cost;
            
            d[i][j] = qMin(deletion, qMin(insertion, substitution));
        }
    }
    
    return d[len1][len2];
}

} // namespace Remus
