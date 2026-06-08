#pragma once

#include <QFile>
#include <QString>
#include <QTemporaryDir>

namespace Remus {
namespace TestFixtures {

    inline QString writeGenesisDat(const QTemporaryDir &dir) {
        static const char kGenesisDat[]
            = "clrmamepro (\n"
              "    name \"Sega - Mega Drive - Genesis\"\n"
              "    description \"Genesis test DAT\"\n"
              "    version \"2026.01.01\"\n"
              ")\n"
              "game (\n"
              "    name \"Sonic The Hedgehog (USA, Europe)\"\n"
              "    description \"Sonic The Hedgehog\"\n"
              "    serial \"00001009-00\"\n"
              "    rom ( name \"Sonic The Hedgehog (USA, Europe).md\" size 524288 crc F9394E97 md5 "
              "1BC674BE034E43C96B86487AC69D9293 sha1 6DDB7DE1E17E7F6CDB88927BD906352030DAA194 )\n"
              ")\n";

        const QString path = dir.filePath(QStringLiteral("Sega - Mega Drive - Genesis.dat"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return QString();
        }

        if (file.write(kGenesisDat) != qstrlen(kGenesisDat)) {
            return QString();
        }

        file.close();
        return path;
    }

} // namespace TestFixtures
} // namespace Remus

namespace Remus {
namespace TestFixtures {

    // Dreamcast DAT with varied serial formats (bare, MK-, HDR-) for normalization testing
    inline QString writeDreamcastDat(const QTemporaryDir &dir) {
        static const char kDreamcastDat[]
            = "clrmamepro (\n"
              "    name \"Sega - Dreamcast\"\n"
              "    description \"Dreamcast test DAT\"\n"
              "    version \"2026.01.01\"\n"
              ")\n"
              "game (\n"
              "    name \"Sonic Adventure (USA)\"\n"
              "    description \"Sonic Adventure (USA)\"\n"
              "    rom ( name \"Sonic Adventure (USA) (Track 3).bin\" size 1185760800 crc 21483B0C md5 "
              "0ED6BE8974F59595B3A955E2CA91E3BB sha1 71A8A044079505B37E173643B6CB5AD6D6942511 serial \"51000\" )\n"
              ")\n"
              "game (\n"
              "    name \"Sonic Adventure (Europe) (En,Ja,Fr,De,Es)\"\n"
              "    description \"Sonic Adventure (Europe)\"\n"
              "    rom ( name \"Sonic Adventure (Europe) (En,Ja,Fr,De,Es) (Track 3).bin\" size 1185760800 crc CB447185 "
              "md5 4DF671C2091877BC3DD9B8907649DED4 sha1 3211DBDB233C12D8D5AF852C05F6C2C0D3BA3C1E serial "
              "\"MK-51000-50\" )\n"
              ")\n"
              "game (\n"
              "    name \"Sonic Adventure (Japan)\"\n"
              "    description \"Sonic Adventure (Japan)\"\n"
              "    rom ( name \"Sonic Adventure (Japan) (Track 3).bin\" size 1185760800 crc 4E7E341A md5 "
              "DFA7A7DF2B6C22128F3AFE4697CC4433 sha1 C98509D77792BE67791B15F762A2019AA85CFBBA serial \"HDR-0001\" )\n"
              ")\n";

        const QString path = dir.filePath(QStringLiteral("Sega - Dreamcast.dat"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return QString();
        }

        if (file.write(kDreamcastDat) != qstrlen(kDreamcastDat)) {
            return QString();
        }

        file.close();
        return path;
    }

} // namespace TestFixtures
} // namespace Remus