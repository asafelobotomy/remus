#include <QTest>
#include "../src/core/result.h"

using namespace Remus;

struct TestData {
    int id = 0;
    QString name;
};

class ResultTest : public QObject {
    Q_OBJECT

private slots:
    void testOkHoldsValue() {
        auto r = Result<int>::ok(42);
        QVERIFY(r.hasValue());
        QVERIFY(static_cast<bool>(r));
        QCOMPARE(*r, 42);
        QCOMPARE(r.value(), 42);
        QVERIFY(r.error().isEmpty());
    }

    void testFailHoldsError() {
        auto r = Result<int>::fail("something broke");
        QVERIFY(!r.hasValue());
        QVERIFY(!static_cast<bool>(r));
        QCOMPARE(r.error(), QStringLiteral("something broke"));
    }

    void testValueOr() {
        auto ok = Result<int>::ok(7);
        QCOMPARE(ok.valueOr(99), 7);

        auto err = Result<int>::fail("nope");
        QCOMPARE(err.valueOr(99), 99);
    }

    void testStructValue() {
        TestData d;
        d.id = 1;
        d.name = "Test";
        auto r = Result<TestData>::ok(d);
        QVERIFY(r);
        QCOMPARE(r->id, 1);
        QCOMPARE(r->name, QStringLiteral("Test"));
    }

    void testMoveSemantics() {
        auto r = Result<QString>::ok("hello");
        QString s = std::move(r).value();
        QCOMPARE(s, QStringLiteral("hello"));
    }
};

QTEST_MAIN(ResultTest)
#include "test_result.moc"
