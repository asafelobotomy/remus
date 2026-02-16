// Test for system name resolution fix
#include "../src/core/database.h"
#include <QCoreApplication>
#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>

using namespace Remus;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    Database db;
    if (!db.initialize(":memory:")) {
        qCritical() << "Failed to initialize database";
        return 1;
    }
    
    qDebug() << "\n=== Testing System Name Resolution ===\n";
    
    // Insert a test system
    QSqlQuery insert(db.database());
    insert.prepare(R"(
        INSERT INTO systems (name, display_name, manufacturer, extensions, preferred_hash)
        VALUES (?, ?, ?, ?, ?)
    )");
    insert.addBindValue("NES");
    insert.addBindValue("Nintendo Entertainment System");
    insert.addBindValue("Nintendo");
    insert.addBindValue("['.nes', '.unf']");
    insert.addBindValue("CRC32");
    
    if (!insert.exec()) {
        qCritical() << "Failed to insert test system:" << insert.lastError().text();
        return 1;
    }
    
    int systemId = insert.lastInsertId().toInt();
    qDebug() << "✓ Inserted test system with ID:" << systemId;
    
    // Test the query that MatchController::getSystemName() uses
    QSqlQuery query(db.database());
    query.prepare("SELECT name FROM systems WHERE id = ?");
    query.addBindValue(systemId);
    
    QString systemName;
    if (query.exec() && query.next()) {
        systemName = query.value(0).toString();
        qDebug() << "✓ Query succeeded, retrieved name:" << systemName;
    } else {
        qCritical() << "❌ Query failed:" << query.lastError().text();
        return 1;
    }
    
    // Verify correctness
    if (systemName == "NES") {
        qDebug() << "\n✅ SUCCESS: System name resolution works correctly!";
        qDebug() << "   - Inserted: NES (ID" << systemId << ")";
        qDebug() << "   - Retrieved: " << systemName;
        return 0;
    } else {
        qCritical() << "\n❌ FAILED: Expected 'NES', got '" << systemName << "'";
        return 1;
    }
}
