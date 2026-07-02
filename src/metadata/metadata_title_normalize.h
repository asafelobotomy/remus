#pragma once

#include <QString>
#include <QStringList>

namespace Remus {
namespace MetadataTitleNormalize {

    QString normalizeMetadataTitle(const QString &title);
    QString metadataTitleMatchTokens(const QString &title);
    QStringList metadataTitleIndexKeys(const QString &title);

} // namespace MetadataTitleNormalize
} // namespace Remus
