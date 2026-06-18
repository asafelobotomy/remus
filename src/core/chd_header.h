#pragma once

#include <QString>

namespace Remus {

/**
 * @brief Disc-index SHA1 read directly from a CHD file header (no chdman spawn).
 */
struct ChdHeaderDigest {
    int version = 0;
    QString sha1; ///< Lowercase hex, 40 chars when valid
    bool valid = false;
};

/// Read header SHA1 for CHD v5 files. Returns invalid for unknown/legacy layouts.
ChdHeaderDigest readChdHeaderDigest(const QString &chdPath);

} // namespace Remus
