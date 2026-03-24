#pragma once

#include <QCommandLineParser>
#include <QString>

#include "../services/mod_catalog_provider.h"

struct ListedMod {
    Remus::ModEntry mod;
    QString         matchScope;
};

struct ModQueryOptions {
    QString systemFilter;
    QString authorFilter;
    QString typeFilter;
    QString formatFilter;
    QString sourceUrlFilter;
    QString sortBy;
    double  minRating = -1.0;
    int     minDownloads = -1;
    bool    jsonOutput = false;
    bool    allowSystemFallback = true;
};

bool parseModQueryOptions(const QCommandLineParser &parser,
                          ModQueryOptions          &options,
                          QString                  &error);

QList<Remus::ModEntry> filterCatalogMods(const QList<Remus::ModEntry> &mods,
                                         const ModQueryOptions         &options);

QList<ListedMod> withScope(const QList<Remus::ModEntry> &mods,
                           const QString                &scope);

QList<ListedMod> sortListedMods(const QList<ListedMod> &mods,
                                const ModQueryOptions  &options);

QString describeActiveFilters(const ModQueryOptions &options);