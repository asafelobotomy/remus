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
///  3. QSettings (plain settings, legacy path)
///  4. OS keychain via SecretStore
///
/// Returns an empty string if the key is not found in any source.
QString get(const QString &key, const QString &jsonFilePath = {});

} // namespace CredentialManager
} // namespace Remus
