#include <QtTest/QtTest>

#include <QSqlError>
#include <QSqlQuery>

#include "../src/core/database.h"

using namespace Remus;

class SystemNameResolutionTest : public QObject {
    Q_OBJECT

private slots:
    void lookupBySystemIdReturnsInsertedName();
};

void SystemNameResolutionTest::lookupBySystemIdReturnsInsertedName() {
    Database db;
    QVERIFY(db.initialize(QStringLiteral(":memory:")));

    QSqlQuery insert(db.database());
    insert.prepare(QStringLiteral(
        "INSERT INTO systems (name, display_name, manufacturer, extensions, preferred_hash) "
        "VALUES (?, ?, ?, ?, ?)"));
    insert.addBindValue(QStringLiteral("TEST_NES"));
    insert.addBindValue(QStringLiteral("Test Nintendo Entertainment System"));
    insert.addBindValue(QStringLiteral("Nintendo"));
    insert.addBindValue(QStringLiteral("['.nes', '.unf']"));
    insert.addBindValue(QStringLiteral("CRC32"));
    QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));

    const int systemId = insert.lastInsertId().toInt();
    QVERIFY(systemId > 0);

    QSqlQuery query(db.database());
    query.prepare(QStringLiteral("SELECT name FROM systems WHERE id = ?"));
    query.addBindValue(systemId);
    QVERIFY2(query.exec() && query.next(), qPrintable(query.lastError().text()));
    QCOMPARE(query.value(0).toString(), QStringLiteral("TEST_NES"));
}

QTEST_MAIN(SystemNameResolutionTest)

#include "test_system_name_resolution.moc"
