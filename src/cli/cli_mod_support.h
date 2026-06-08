#pragma once

#include <QCommandLineParser>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "../core/database.h"
#include "../services/mod_catalog_provider.h"

struct ListedMod {
    Remus::ModEntry mod;
    QString matchScope;
};

struct ModQueryOptions {
    QString systemFilter;
    QString authorFilter;
    QString typeFilter;
    QString formatFilter;
    QString sourceUrlFilter;
    QString sortBy;
    double minRating = -1.0;
    int minDownloads = -1;
    bool jsonOutput = false;
    bool allowSystemFallback = true;
};

bool parseModQueryOptions(const QCommandLineParser &parser, ModQueryOptions &options, QString &error);

QList<Remus::ModEntry> filterCatalogMods(const QList<Remus::ModEntry> &mods, const ModQueryOptions &options);

QList<ListedMod> withScope(const QList<Remus::ModEntry> &mods, const QString &scope);

QList<ListedMod> sortListedMods(const QList<ListedMod> &mods, const ModQueryOptions &options);

QString describeActiveFilters(const ModQueryOptions &options);

QJsonObject listedModToJson(const ListedMod &row);

QJsonObject installedModToJson(const Remus::Database::ModInstallationRecord &record, const QString &baseFilename);

QJsonObject systemCountToJson(const QString &system, int count);

void printJsonArray(const QJsonArray &array);

void printJsonObject(const QJsonObject &object);

void printModList(const QList<ListedMod> &mods);

void printModDetails(const Remus::ModEntry &mod);

bool loadModCatalog(struct CliContext &ctx, Remus::ModCatalogProvider &catalog, QString &error);