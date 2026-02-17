#include <QtTest/QtTest>
#include "../src/metadata/filename_normalizer.h"

using namespace Remus::Metadata;

class TestFilenameNormalizer : public QObject
{
    Q_OBJECT

private slots:
    void testBasicExtensionRemoval()
    {
        QCOMPARE(FilenameNormalizer::normalize("Sonic.md"), QString("Sonic"));
        QCOMPARE(FilenameNormalizer::normalize("Doom.smc"), QString("Doom"));
        QCOMPARE(FilenameNormalizer::normalize("Final Fantasy.cue"), QString("Final Fantasy"));
    }

    void testRegionTagRemoval()
    {
        QCOMPARE(FilenameNormalizer::normalize("Sonic The Hedgehog (USA).md"), QString("Sonic The Hedgehog"));
        QCOMPARE(FilenameNormalizer::normalize("Pokemon Red (USA, Europe).gbc"), QString("Pokemon Red"));
        QCOMPARE(FilenameNormalizer::normalize("Metal Gear (Japan).nes"), QString("Metal Gear"));
    }

    void testBracketTagRemoval()
    {
        QCOMPARE(FilenameNormalizer::normalize("Zelda [!].nes"), QString("Zelda"));
        QCOMPARE(FilenameNormalizer::normalize("Mario [Classics].smc"), QString("Mario"));
        QCOMPARE(FilenameNormalizer::normalize("Metroid [b1].nes"), QString("Metroid"));
    }

    void testUnderscoreReplacement()
    {
        QCOMPARE(FilenameNormalizer::normalize("Super_Mario_World.smc"), QString("Super Mario World"));
        QCOMPARE(FilenameNormalizer::normalize("Street_Fighter_II.md"), QString("Street Fighter II"));
    }

    void testDotReplacement()
    {
        QCOMPARE(FilenameNormalizer::normalize("Super.Mario.World.smc"), QString("Super Mario World"));
    }

    void testComplexExample()
    {
        // Real-world example from the conversation
        QCOMPARE(
            FilenameNormalizer::normalize("Sonic The Hedgehog (USA, Europe).md"),
            QString("Sonic The Hedgehog")
        );
        
        // Another complex example
        QCOMPARE(
            FilenameNormalizer::normalize("Shin Megami Tensei 1 (English Addendum 1.0)[Automap].smc"),
            QString("Shin Megami Tensei 1")
        );
        
        // Multiple brackets
        QCOMPARE(
            FilenameNormalizer::normalize("Doom [!] [Classics].wad"),
            QString("Doom")
        );
    }

    void testEmptyAndEdgeCases()
    {
        QCOMPARE(FilenameNormalizer::normalize(""), QString(""));
        QCOMPARE(FilenameNormalizer::normalize("NoExtension"), QString("NoExtension"));
        QCOMPARE(FilenameNormalizer::normalize("   Spaces   "), QString("Spaces"));
    }

    void testMultipleSpaces()
    {
        // simplified() should collapse multiple spaces
        QCOMPARE(
            FilenameNormalizer::normalize("Super  Mario   World.smc"),
            QString("Super Mario World")
        );
    }
};

QTEST_MAIN(TestFilenameNormalizer)
#include "test_filename_normalizer.moc"
