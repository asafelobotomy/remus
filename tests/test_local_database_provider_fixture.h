#pragma once

#include <QFile>
#include <QString>
#include <QTemporaryDir>

namespace Remus {
namespace TestFixtures {

inline QString writeGenesisDat(const QTemporaryDir &dir)
{
    static const char kGenesisDat[] =
        "clrmamepro (\n"
        "    name \"Sega - Mega Drive - Genesis\"\n"
        "    description \"Genesis test DAT\"\n"
        "    version \"2026.01.01\"\n"
        ")\n"
        "game (\n"
        "    name \"Sonic The Hedgehog (USA, Europe)\"\n"
        "    description \"Sonic The Hedgehog\"\n"
        "    serial \"00001009-00\"\n"
        "    rom ( name \"Sonic The Hedgehog (USA, Europe).md\" size 524288 crc F9394E97 md5 1BC674BE034E43C96B86487AC69D9293 sha1 6DDB7DE1E17E7F6CDB88927BD906352030DAA194 )\n"
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