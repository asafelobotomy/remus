#pragma once

#include <QtGlobal>
#include <QString>

namespace Remus {

/**
 * @brief Read-model for a compendium @c game_disc_sets row (+ track count).
 */
struct CompendiumDiscSet {
    qint64 discSetId = 0;
    QString gameId;
    QString setKey;
    int discNumber = 0;
    int discCount = 0;
    QString setVariant;
    QString setRole;
    QString titleDisc;
    QString primaryContentSha1;
    int trackCount = 0;
};

} // namespace Remus
