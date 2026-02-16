#include <QCoreApplication>
#include <QDebug>
#include "src/metadata/clrmamepro_parser.h"

using namespace Remus;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    QString datFile = "/home/solon/Documents/remus/data/databases/Sega - Mega Drive - Genesis.dat";
    
    qDebug() << "Parsing" << datFile;
    
    QList<ClrMameProEntry> entries = ClrMameProParser::parse(datFile);
    
    qDebug() << "Total entries:" << entries.size();
    
    if (entries.size() > 0) {
        qDebug() << "\nFirst 3 entries:";
        for (int i = 0; i < qMin(3, entries.size()); i++) {
            const ClrMameProEntry &entry = entries[i];
            qDebug() << "  Name:" << entry.gameName;
            qDebug() << "  CRC32:" << entry.crc32;
            qDebug() << "  Size:" << entry.size;
            qDebug() << "";
        }
        
        // Look for Sonic
        for (const ClrMameProEntry &entry : entries) {
            if (entry.gameName.contains("Sonic The Hedgehog", Qt::CaseInsensitive)) {
                qDebug() << "Found Sonic The Hedgehog!";
                qDebug() << "  Name:" << entry.gameName;
                qDebug() << "  CRC32:" << entry.crc32;
                qDebug() << "  Region:" << entry.region;
                break;
            }
        }
    }
    
    return 0;
}
