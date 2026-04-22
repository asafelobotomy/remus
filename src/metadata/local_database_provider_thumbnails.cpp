#include "local_database_provider.h"
#include "thumbnail_url_helper.h"

namespace Remus {

// These static methods delegate to the shared ThumbnailUrlHelper so that the
// same URL generation logic is available to CompendiumProvider and bundle
// helpers without a LocalDatabaseProvider dependency.

QString LocalDatabaseProvider::sanitizeThumbnailName(const QString &gameName)
{
    return Metadata::ThumbnailUrlHelper::sanitizeThumbnailName(gameName);
}

QString LocalDatabaseProvider::stripLanguageTags(const QString &gameName)
{
    return Metadata::ThumbnailUrlHelper::stripLanguageTags(gameName);
}

QStringList LocalDatabaseProvider::generateThumbnailCandidates(const QString &systemName,
                                                                const QString &gameName,
                                                                const QString &type)
{
    return Metadata::ThumbnailUrlHelper::generateThumbnailCandidates(systemName, gameName, type);
}

QString LocalDatabaseProvider::buildThumbnailUrl(const QString &systemName,
                                                  const QString &gameName,
                                                  const QString &type)
{
    return Metadata::ThumbnailUrlHelper::buildThumbnailUrl(systemName, gameName, type);
}

} // namespace Remus
