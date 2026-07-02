#pragma once

#include "../metadata/metadata_provider.h"

#include <QDateTime>
#include <QList>
#include <QString>

namespace CompendiumPlatformIndexCache {

static constexpr int kDefaultTtlDays = 7;

QString cacheRootDir();

bool loadPlatformIndex(const QString &providerKey, const QString &platformKey, QList<Remus::GameMetadata> &out,
    QDateTime *cachedAt = nullptr);

bool storePlatformIndex(
    const QString &providerKey, const QString &platformKey, const QList<Remus::GameMetadata> &games);

} // namespace CompendiumPlatformIndexCache
