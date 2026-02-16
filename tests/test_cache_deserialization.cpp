// Quick test for metadata cache deserialization fix
#include "../src/metadata/metadata_cache.h"
#include "../src/core/database.h"
#include "../src/core/constants/match_methods.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>

using namespace Remus;
using namespace Remus::Constants;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    // Create temporary database
    Database db;
    if (!db.initialize(":memory:")) {
        qCritical() << "Failed to initialize database";
        return 1;
    }
    
    MetadataCache cache(db.database());
    
    // Create test metadata
    GameMetadata original;
    original.id = "12345";
    original.title = "Super Mario Bros.";
    original.system = "NES";
    original.region = "USA";
    original.publisher = "Nintendo";
    original.developer = "Nintendo R&D4";
    original.genres << "Platform" << "Action";
    original.releaseDate = "1985-09-13";
    original.description = "Classic platformer";
    original.players = 2;
    original.rating = 9.5f;
    original.providerId = "screenscraper";
    original.boxArtUrl = "https://example.com/art.jpg";
    original.matchMethod = MatchMethods::HASH;
    original.matchScore = 1.0f;
    original.externalIds["igdb"] = "999";
    original.fetchedAt = QDateTime::currentDateTime();
    
    qDebug() << "\n=== Testing Metadata Cache Deserialization ===\n";
    qDebug() << "Original metadata:";
    qDebug() << "  Title:" << original.title;
    qDebug() << "  Genres:" << original.genres;
    qDebug() << "  Rating:" << original.rating;
    qDebug() << "  External IDs:" << original.externalIds;
    
    // Store metadata
    QString hash = "811b027eaf99c2def7b933c5208636de";
    if (!cache.store(original, hash, "NES")) {
        qCritical() << "\n❌ FAILED: Could not store metadata";
        return 1;
    }
    qDebug() << "\n✓ Metadata stored in cache";
    
    // Retrieve by hash
    GameMetadata retrieved = cache.getByHash(hash, "NES");
    
    // Verify deserialization
    bool success = true;
    
    if (retrieved.title != original.title) {
        qCritical() << "❌ Title mismatch:" << retrieved.title << "!=" << original.title;
        success = false;
    }
    
    if (retrieved.genres != original.genres) {
        qCritical() << "❌ Genres mismatch:" << retrieved.genres << "!=" << original.genres;
        success = false;
    }
    
    if (retrieved.rating != original.rating) {
        qCritical() << "❌ Rating mismatch:" << retrieved.rating << "!=" << original.rating;
        success = false;
    }
    
    if (retrieved.externalIds != original.externalIds) {
        qCritical() << "❌ External IDs mismatch:" << retrieved.externalIds << "!=" << original.externalIds;
        success = false;
    }
    
    if (retrieved.matchScore != original.matchScore) {
        qCritical() << "❌ Match score mismatch:" << retrieved.matchScore << "!=" << original.matchScore;
        success = false;
    }
    
    if (success) {
        qDebug() << "\n✓ Retrieved metadata:";
        qDebug() << "  Title:" << retrieved.title;
        qDebug() << "  Genres:" << retrieved.genres;
        qDebug() << "  Rating:" << retrieved.rating;
        qDebug() << "  External IDs:" << retrieved.externalIds;
        qDebug() << "  Fetched at:" << retrieved.fetchedAt.toString(Qt::ISODate);
        qDebug() << "\n✅ SUCCESS: Cache deserialization works correctly!";
        return 0;
    } else {
        qCritical() << "\n❌ FAILED: Cache deserialization has errors";
        return 1;
    }
}
