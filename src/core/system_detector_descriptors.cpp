#include "system_detector.h"
#include "disc_magic_detector.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>

namespace Remus {

QString SystemDetector::detectFromCueDescriptor(const QString &path, const QStringList &candidates, QStringList *evidence) const
{
    QFile cueFile(path);
    if (!cueFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (evidence) evidence->append(QStringLiteral("cue-descriptor:open-failed"));
        return {};
    }

    // Match both quoted: FILE "name.bin" BINARY
    // and unquoted:     FILE name.bin BINARY
    static const QRegularExpression cueFileQuotedRx(
        QStringLiteral("^\\s*FILE\\s+\"([^\"]+)\""),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression cueFileUnquotedRx(
        QStringLiteral("^\\s*FILE\\s+(\\S+)"),
        QRegularExpression::CaseInsensitiveOption);

    QTextStream in(&cueFile);
    QString firstTrack;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        QRegularExpressionMatch m = cueFileQuotedRx.match(line);
        if (!m.hasMatch())
            m = cueFileUnquotedRx.match(line);
        if (m.hasMatch()) {
            firstTrack = m.captured(1);
            break;
        }
    }

    if (firstTrack.isEmpty()) {
        if (evidence) evidence->append(QStringLiteral("cue-descriptor:no-track-found"));
        return {};
    }

    const QFileInfo trackInfo(QFileInfo(path).dir(), firstTrack);
    const QString trackExt = QStringLiteral(".") + trackInfo.suffix().toLower();
    if (evidence) evidence->append(QStringLiteral("cue-descriptor:track-ext=") + trackExt);

    // Narrow the CUE candidates to only those that also support the track extension.
    const QStringList trackCandidates = getCandidatesForExtension(trackExt);
    QStringList narrowed;
    for (const QString &c : candidates) {
        if (trackCandidates.contains(c))
            narrowed.append(c);
    }
    const QStringList &effective = narrowed.isEmpty() ? candidates : narrowed;
    if (evidence) evidence->append(QStringLiteral("cue-descriptor:narrowed=") + effective.join(QStringLiteral(",")));

    if (effective.size() == 1) {
        return effective.first();
    }

    // If still ambiguous and the track file exists, probe disc magic.
    const QString trackPath = trackInfo.absoluteFilePath();
    if (QFileInfo::exists(trackPath)) {
        const DiscHeaderInfo discInfo = DiscMagicDetector::detect(trackPath);
        if (discInfo.detected && !discInfo.systemName.isEmpty() && effective.contains(discInfo.systemName)) {
            if (evidence) evidence->append(QStringLiteral("cue-descriptor:disc-magic=") + discInfo.systemName);
            return discInfo.systemName;
        }
    }

    return {};
}

QString SystemDetector::detectFromM3uDescriptor(const QString &path, const QStringList &candidates, QStringList *evidence) const
{
    QFile m3uFile(path);
    if (!m3uFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (evidence) evidence->append(QStringLiteral("m3u-descriptor:open-failed"));
        return {};
    }

    QTextStream in(&m3uFile);
    QStringList entries;
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        entries.append(line);
    }
    m3uFile.close();

    if (entries.isEmpty()) {
        if (evidence) evidence->append(QStringLiteral("m3u-descriptor:no-entry"));
        return {};
    }

    const QDir m3uDir = QFileInfo(path).dir();

    // Scan all entries; stop at the first one that unambiguously resolves a system.
    for (const QString &rawEntry : entries) {
        // Normalise Windows paths (e.g. "D:\path\disc.gdi") to portable form.
        QString entry = rawEntry;
        if (entry.size() >= 2 && entry[1] == QLatin1Char(':'))
            entry = entry.mid(2);                           // strip drive letter
        entry.replace(QLatin1Char('\\'), QLatin1Char('/'));  // backslash → forward slash

        const QFileInfo entryInfo(m3uDir, entry);
        const QString entryExt = QStringLiteral(".") + entryInfo.suffix().toLower();
        if (evidence) evidence->append(QStringLiteral("m3u-descriptor:entry-ext=") + entryExt);

        const QStringList entryCandidates = getCandidatesForExtension(entryExt);
        QStringList narrowed;
        for (const QString &c : candidates) {
            if (entryCandidates.contains(c))
                narrowed.append(c);
        }
        const QStringList &effective = narrowed.isEmpty() ? candidates : narrowed;

        if (effective.size() == 1) {
            if (evidence) evidence->append(QStringLiteral("m3u-descriptor:narrowed=") + effective.first());
            return effective.first();
        }

        // If the entry is a CUE file, recurse one level to parse its tracks.
        if (entryExt == QStringLiteral(".cue")) {
            const QString byCue = detectFromCueDescriptor(entryInfo.absoluteFilePath(), effective, evidence);
            if (!byCue.isEmpty())
                return byCue;
        }

        // Try disc magic on the referenced entry file if it exists on disk.
        const QString entryPath = entryInfo.absoluteFilePath();
        if (QFileInfo::exists(entryPath)) {
            const DiscHeaderInfo discInfo = DiscMagicDetector::detect(entryPath);
            if (discInfo.detected && !discInfo.systemName.isEmpty() && effective.contains(discInfo.systemName)) {
                if (evidence) evidence->append(QStringLiteral("m3u-descriptor:disc-magic=") + discInfo.systemName);
                return discInfo.systemName;
            }
        }
        // No resolution from this entry; continue to next.
    }

    if (evidence) evidence->append(QStringLiteral("m3u-descriptor:all-entries-unresolved"));
    return {};
}

} // namespace Remus
