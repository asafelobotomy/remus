#pragma once

#include <QString>

namespace Remus {

struct PatchedRomInfo {
    QString rawName;
    QString baseTitle;
    QString patchName;
    QString fileType = QStringLiteral("official");
    bool isPatched = false;
};

class PatchedRomParser {
public:
    static PatchedRomInfo parse(const QString &nameOrPath);

private:
    PatchedRomParser() = delete;
};

} // namespace Remus