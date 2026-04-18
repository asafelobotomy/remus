#include "rom_bundler.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QRegularExpression>
#include <QTextStream>

#include "constants/constants.h"
#include "constants/files.h"
#include "constants/systems.h"
#include "logging_categories.h"
#include "rvz_converter.h"

#undef qDebug
#undef qInfo
#undef qWarning
#undef qCritical
#define qDebug()    qCDebug(logCore)
#define qInfo()     qCInfo(logCore)
#define qWarning()  qCWarning(logCore)
#define qCritical() qCCritical(logCore)

namespace Remus {

// ─── constants ────────────────────────────────────────────────────────────────

static constexpr const char *MARKER_FILENAME = ".remus.md";

// ─── construction ─────────────────────────────────────────────────────────────

RomBundler::RomBundler(Database &db, QObject *parent)
    : QObject(parent)
    , m_db(db)
{
}

// ─── public API ───────────────────────────────────────────────────────────────

bool RomBundler::isAlreadyBundled(const QString &archivePath)
{
    ArchiveInfo info = m_extractor.getArchiveInfo(archivePath);
    for (QString entry : info.contents) {
        entry = entry.trimmed().replace('\\', '/');
        while (entry.startsWith("./")) {
            entry.remove(0, 2);
        }
        if (entry == QLatin1String(MARKER_FILENAME)) {
            return true;
        }
    }
    return false;
}

// ─── bundleStaged ─────────────────────────────────────────────────────────────

} // namespace Remus
