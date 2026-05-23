#pragma once
#include <QString>

namespace Remus {

/// Synchronous OS-backed credential store wrapper (qt6keychain).
/// Secrets are stored in the platform keychain (Secret Service on Linux,
/// Keychain on macOS, Credential Manager on Windows) rather than in plain
/// QSettings files.
class SecretStore {
public:
    /// Structured result from readWithStatus().
    struct ReadResult {
        enum class Status {
            Found,        ///< Key exists and value was returned.
            NotFound,     ///< Key does not exist in the keychain.
            BackendError  ///< Keychain backend unavailable or other I/O error.
        };
        Status  status;
        QString value;
        QString errorMessage; ///< Non-empty only when status == BackendError.
    };

    /// Read a secret and return a structured result that distinguishes
    /// "not found" from a backend error.  Prefer this over read() when the
    /// caller needs to decide whether to fall back to a less-secure store.
    static ReadResult readWithStatus(const QString &key);

    /// Convenience wrapper.  Returns the value on Found, empty string on
    /// NotFound or BackendError.  BackendError is logged as a warning.
    static QString read(const QString &key);

    /// Write a secret. Returns false if the keychain is unavailable or the
    /// write otherwise fails.
    static bool write(const QString &key, const QString &value);

    /// Remove a secret from the keychain. Silent no-op if not present.
    static void remove(const QString &key);
};

} // namespace Remus
