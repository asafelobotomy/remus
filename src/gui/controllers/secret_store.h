#pragma once
#include <QString>

namespace Remus {

/// Synchronous OS-backed credential store wrapper (qt6keychain).
/// Secrets are stored in the platform keychain (Secret Service on Linux,
/// Keychain on macOS, Credential Manager on Windows) rather than in plain
/// QSettings files.
class SecretStore {
public:
    /// Read a secret. Returns an empty string if the key is not found or if
    /// the keychain backend is unavailable.
    static QString read(const QString &key);

    /// Write a secret. Returns false if the keychain is unavailable or the
    /// write otherwise fails.
    static bool write(const QString &key, const QString &value);

    /// Remove a secret from the keychain. Silent no-op if not present.
    static void remove(const QString &key);
};

} // namespace Remus
