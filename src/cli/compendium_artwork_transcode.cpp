#include "compendium_artwork_transcode.h"
#include "compendium_cwebp_resolver.h"

#include <QFile>
#include <QImageWriter>
#include <QProcess>

namespace CompendiumArtworkTranscode {

namespace {

    bool webpWriterAvailable() {
        static const bool available = QImageWriter::supportedImageFormats().contains("webp");
        return available;
    }

} // namespace

void appendCwebpEncodingArgs(QStringList &args, bool lossless, int snapQuality) {
    if (lossless) {
        args << QStringLiteral("-lossless") << QStringLiteral("-m") << QStringLiteral("6");
    } else {
        args << QStringLiteral("-q") << QString::number(qBound(1, snapQuality, 100)) << QStringLiteral("-m")
             << QStringLiteral("4");
    }
}

void configureLosslessPngWriter(QImageWriter &writer) {
    writer.setCompression(9);
}

bool storedAssetNeedsUpgrade(const QString &storedMimeType, const QString &targetMimeType) {
    if (storedMimeType == targetMimeType) {
        return false;
    }
    return storedMimeType == QStringLiteral("image/png") && targetMimeType == QStringLiteral("image/webp");
}

bool transcodeImageToWebp(const QImage &image, const QString &destPath, const QString &repoRoot, bool lossless,
    int snapQuality, QString &error) {
    if (image.isNull()) {
        error = QStringLiteral("Failed to transcode null image");
        return false;
    }

    if (webpWriterAvailable()) {
        QImageWriter writer(destPath, "webp");
        if (lossless) {
            writer.setQuality(100);
        } else {
            writer.setQuality(qBound(1, snapQuality, 100));
        }
        if (!writer.write(image)) {
            error = QStringLiteral("QImageWriter failed for %1: %2").arg(destPath, writer.errorString());
            return false;
        }
        return true;
    }

    const QString tempPng = destPath + QStringLiteral(".tmp.png");
    {
        QImageWriter pngWriter(tempPng, "png");
        configureLosslessPngWriter(pngWriter);
        if (!pngWriter.write(image)) {
            error = QStringLiteral("Failed to write temp PNG for cwebp: %1").arg(tempPng);
            return false;
        }
    }

    QStringList args;
    appendCwebpEncodingArgs(args, lossless, snapQuality);
    args << tempPng << QStringLiteral("-o") << destPath;

    const QString cwebpBin = CompendiumCwebp::resolveCwebpExecutable(repoRoot);
    if (cwebpBin.isEmpty()) {
        QFile::remove(tempPng);
        error = QStringLiteral(
            "cwebp not found; run 'npm install' in repo root (cwebp-bin) or pass --thumbnail-format png");
        return false;
    }

    QProcess proc;
    proc.start(cwebpBin, args);
    if (proc.error() == QProcess::FailedToStart) {
        QFile::remove(tempPng);
        error = QStringLiteral("cwebp failed to start: %1").arg(cwebpBin);
        return false;
    }
    if (!proc.waitForFinished(120000) || proc.exitCode() != 0) {
        QFile::remove(tempPng);
        error = QStringLiteral("cwebp failed: %1").arg(QString::fromUtf8(proc.readAllStandardError()));
        return false;
    }
    QFile::remove(tempPng);
    return true;
}

} // namespace CompendiumArtworkTranscode
