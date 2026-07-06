#pragma once
#include <QString>

namespace Remus {
namespace CredentialManager {

    /// Look up a credential by its QSettings key (e.g. "igdb/client_id").
    ///
    /// Lookup order:
    ///  1. JSON credentials file at @p jsonFilePath (key is split on '/' for
    ///     outer/inner JSON object access)
    ///  2. Environment variable from the built-in env-var table
    ///  3. OS keychain via SecretStore (BackendError halts chain — no plaintext
    ///     downgrade; NotFound continues to legacy QSettings)
    ///  4. QSettings (plain settings, legacy path)
    ///
    /// Returns an empty string if the key is not found in any source.
    QString get(const QString &key, const QString &jsonFilePath = { });

    /// Read a credential from @p jsonFilePath only (no env/keychain/settings fallback).
    QString getFromFile(const QString &key, const QString &jsonFilePath);

    /// Move legacy plaintext QSettings secrets into the OS keychain when empty.
    void migrateLegacySecrets();

} // namespace CredentialManager
} // namespace Remus
