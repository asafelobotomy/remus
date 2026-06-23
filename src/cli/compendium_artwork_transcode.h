#pragma once

#include <QImage>
#include <QImageWriter>
#include <QString>
#include <QStringList>

namespace CompendiumArtworkTranscode {

/** cwebp -m 6 for lossless; -m 4 for lossy (plan: maximize lossless compression). */
void appendCwebpEncodingArgs(QStringList &args, bool lossless, int snapQuality);

void configureLosslessPngWriter(QImageWriter &writer);

bool storedAssetNeedsUpgrade(const QString &storedMimeType, const QString &targetMimeType);

bool transcodeImageToWebp(const QImage &image, const QString &destPath, const QString &repoRoot, bool lossless,
    int snapQuality, QString &error);

} // namespace CompendiumArtworkTranscode
