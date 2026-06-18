#pragma once

#include <QString>
#include <QStringList>
#include <QList>

namespace Remus {

struct DiscTrackCompleteness {
    int discNumber = 0;
    int expectedTracks = 0;
    int ownedTracks = 0;
    QStringList missingRomNames;
};

struct DiscSetCompletenessReport {
    QString setKey;
    QString compendiumGameId;
    QString titleDisc;
    int discCount = 0;
    QList<int> ownedDiscNumbers;
    QList<int> missingDiscNumbers;
    QStringList warnings;
    QList<DiscTrackCompleteness> trackGaps;
};

} // namespace Remus
