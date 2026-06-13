#include <QtTest/QtTest>

#include "../src/core/match_utils.h"
#include "../src/core/database_types.h"

using namespace Remus;

class MatchUtilsTest : public QObject {
    Q_OBJECT

private slots:
    void deriveMatchingDisplayName_usesBaseTitleForPatchedRom();
};

void MatchUtilsTest::deriveMatchingDisplayName_usesBaseTitleForPatchedRom() {
    FileRecord file;
    file.filename = QStringLiteral("Shin Megami Tensei (Japan) [Fan Translation v1.2].sfc");
    const QString displayName = deriveMatchingDisplayName(file);
    QCOMPARE(displayName, QStringLiteral("Shin Megami Tensei"));
}

QTEST_MAIN(MatchUtilsTest)
#include "test_match_utils.moc"
