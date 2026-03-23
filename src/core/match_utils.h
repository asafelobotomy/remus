#pragma once

#include <QString>
#include "database.h"

namespace Remus {

QString selectBestMatchHash(const FileRecord &file);
QString deriveMatchingDisplayName(const FileRecord &file);

} // namespace Remus