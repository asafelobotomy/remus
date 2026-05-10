#include "cli_mod_support.h"

#include <QJsonDocument>
#include <QTextStream>

QJsonObject listedModToJson(const ListedMod &row)
{
    const auto &mod = row.mod;

    QJsonObject object;
    object["id"] = mod.id;
    object["title"] = mod.title;
    object["author"] = mod.author;
    object["version"] = mod.version;
    object["description"] = mod.description;
    object["type"] = mod.type;
    object["system"] = mod.system;
    object["format"] = mod.format;
    object["patchUrl"] = mod.patchUrl;
    object["patchSha1"] = mod.patchSha1;
    object["patchSize"] = static_cast<qint64>(mod.patchSize);
    object["baseCrc32"] = mod.baseCrc32;
    object["baseMd5"] = mod.baseMd5;
    object["baseSha1"] = mod.baseSha1;
    object["sourceUrl"] = mod.sourceUrl;
    object["rating"] = mod.rating;
    object["downloads"] = mod.downloads;
    if (!row.matchScope.isEmpty()) {
        object["matchScope"] = row.matchScope;
    }
    return object;
}

QJsonObject installedModToJson(const Remus::Database::ModInstallationRecord &record,
                              const QString                                &baseFilename)
{
    QJsonObject object;
    object["id"] = record.id;
    object["baseFileId"] = record.baseFileId;
    object["patchedFileId"] = record.patchedFileId;
    object["catalogModId"] = record.catalogModId;
    object["modTitle"] = record.modTitle;
    object["modAuthor"] = record.modAuthor;
    object["modVersion"] = record.modVersion;
    object["modType"] = record.modType;
    object["patchFormat"] = record.patchFormat;
    object["patchUrl"] = record.patchUrl;
    object["patchSha1"] = record.patchSha1;
    object["sourceUrl"] = record.sourceUrl;
    object["installedAt"] = record.installedAt.toString(Qt::ISODate);
    object["baseFilename"] = baseFilename;
    return object;
}

QJsonObject systemCountToJson(const QString &system, int count)
{
    QJsonObject object;
    object["system"] = system;
    object["count"] = count;
    return object;
}

void printJsonArray(const QJsonArray &array)
{
    QTextStream stream(stdout);
    stream << QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Indented)).trimmed() << Qt::endl;
}

void printJsonObject(const QJsonObject &object)
{
    QTextStream stream(stdout);
    stream << QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented)).trimmed() << Qt::endl;
}

void printModList(const QList<ListedMod> &mods)
{
    bool showMatchScope = false;
    for (const auto &row : mods) {
        if (!row.matchScope.isEmpty()) {
            showMatchScope = true;
            break;
        }
    }

    if (showMatchScope) {
        qInfo().noquote() << QString("%1  %2  %3  %4  %5  %6")
            .arg("Scope", -8)
            .arg("ID", -20)
            .arg("Title", -40)
            .arg("Type", -14)
            .arg("Format", -8)
            .arg("Rating");
    } else {
        qInfo().noquote() << QString("%1  %2  %3  %4  %5")
            .arg("ID", -20)
            .arg("Title", -40)
            .arg("Type", -14)
            .arg("Format", -8)
            .arg("Rating");
    }

    for (const auto &row : mods) {
        const auto &mod = row.mod;
        if (showMatchScope) {
            qInfo().noquote() << QString("%1  %2  %3  %4  %5  %6")
                .arg(row.matchScope.left(8), -8)
                .arg(mod.id.left(20), -20)
                .arg(mod.title.left(40), -40)
                .arg(mod.type.left(14), -14)
                .arg(mod.format.left(8), -8)
                .arg(QString::number(mod.rating, 'f', 1));
        } else {
            qInfo().noquote() << QString("%1  %2  %3  %4  %5")
                .arg(mod.id.left(20), -20)
                .arg(mod.title.left(40), -40)
                .arg(mod.type.left(14), -14)
                .arg(mod.format.left(8), -8)
                .arg(QString::number(mod.rating, 'f', 1));
        }
    }
}

void printModDetails(const Remus::ModEntry &mod)
{
    qInfo().noquote() << QString("ID:          %1").arg(mod.id);
    qInfo().noquote() << QString("Title:       %1").arg(mod.title);
    qInfo().noquote() << QString("Author:      %1").arg(mod.author);
    qInfo().noquote() << QString("Version:     %1").arg(mod.version);
    qInfo().noquote() << QString("Type:        %1").arg(mod.type);
    qInfo().noquote() << QString("System:      %1").arg(mod.system);
    qInfo().noquote() << QString("Format:      %1").arg(mod.format);
    qInfo().noquote() << QString("Patch URL:   %1").arg(mod.patchUrl);
    qInfo().noquote() << QString("Patch SHA1:  %1").arg(mod.patchSha1.isEmpty() ? "(none)" : mod.patchSha1);
    qInfo().noquote() << QString("Downloads:   %1").arg(mod.downloads);
    qInfo().noquote() << QString("Rating:      %1").arg(QString::number(mod.rating, 'f', 1));
    qInfo().noquote() << QString("Source URL:  %1").arg(mod.sourceUrl.isEmpty() ? "(none)" : mod.sourceUrl);
    if (!mod.description.isEmpty()) {
        qInfo().noquote() << QString("Description: %1").arg(mod.description);
    }
}
