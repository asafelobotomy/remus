#pragma once

#include <QString>

namespace Remus {

/**
 * @brief Canonical and legacy disc-set key helpers.
 *
 * Library rows continue to use @ref legacyLibraryGroupKey for backward compatibility.
 * Compendium ingest uses @ref compute for stable cross-source set identity.
 */
class DiscSetKey {
public:
    /**
     * @brief Canonical compendium set key.
     *
     * @c sha1(system_id|identity_base|region) truncated to 16 hex chars.
     */
    static QString compute(int systemId, const QString &title, const QString &regionCode = QString());

    /// Existing library format: @c baseTitle|systemDisplayName .
    static QString legacyLibraryGroupKey(const QString &labelPath, const QString &systemDisplayName);

    static QString computeFromParsed(int systemId, const QString &identityBase, const QString &regionCode);
};

} // namespace Remus
