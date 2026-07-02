#include "library_exporter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include "constants/constants.h"

namespace Remus {

QList<LibraryExportRow> LibraryExporter::buildRows(Database &db, const QStringList &systemFilters) {
    const QMap<int, Database::MatchResult> matches = db.getAllMatches();
    const QList<FileRecord> files = db.getExistingFiles();

    QList<LibraryExportRow> rows;
    rows.reserve(files.size());
    for (const FileRecord &file : files) {
        if (!matches.contains(file.id))
            continue;
        const QString systemName = db.getSystemDisplayName(file.systemId);
        if (!systemFilters.isEmpty() && !systemFilters.contains(systemName))
            continue;
        rows.append({ file, matches.value(file.id) });
    }
    return rows;
}

QString LibraryExporter::defaultFilename(const QString &format) {
    const QString normalized = format.trimmed().toLower();
    using namespace Constants::Exports;
    if (normalized == Formats::RETROARCH)
        return Files::DEFAULT_RETROARCH_EXPORT;
    if (normalized == Formats::EMUSTATION)
        return Files::ES_GAMELIST;
    if (normalized == Formats::LAUNCHBOX)
        return Files::DEFAULT_LAUNCHBOX_EXPORT;
    if (normalized == Formats::CSV)
        return Files::DEFAULT_CSV_EXPORT;
    return Files::DEFAULT_JSON_EXPORT;
}

QString LibraryExporter::resolveOutputPath(const QString &format, const QString &outputPath) {
    if (outputPath.isEmpty())
        return defaultFilename(format);
    if (QFileInfo(outputPath).isDir())
        return QDir(outputPath).filePath(defaultFilename(format));
    return outputPath;
}

bool LibraryExporter::exportToFile(Database &db, const QString &format, const QString &outputPath,
    const QStringList &systemFilters, QString *error, ProgressCallback onProgress) {
    const QString normalized = format.trimmed().toLower();
    const QList<LibraryExportRow> rows = buildRows(db, systemFilters);
    if (rows.isEmpty()) {
        if (error)
            *error = QStringLiteral("No matched files to export");
        return false;
    }

    const int totalRows = rows.size();
    auto reportProgress = [&](int current) {
        if (onProgress) {
            onProgress(current, totalRows);
        }
    };

    const QString resolvedPath = resolveOutputPath(normalized, outputPath);
    QFile file(resolvedPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Failed to open %1 for writing").arg(resolvedPath);
        return false;
    }

    using namespace Constants::Exports;

    if (normalized == Formats::RETROARCH) {
        QTextStream out(&file);
        for (int i = 0; i < rows.size(); ++i) {
            const auto &row = rows.at(i);
            out << row.file.currentPath << "\n";
            out << (!row.match.gameTitle.isEmpty() ? row.match.gameTitle : row.file.filename) << "\n";
            out << "DETECT\nDETECT\n";
            out << (row.file.crc32.isEmpty() ? "00000000" : row.file.crc32) << "|crc\n";
            out << db.getSystemDisplayName(row.file.systemId) << Files::PLAYLIST_EXTENSION << "\n";
            reportProgress(i + 1);
        }
    } else if (normalized == Formats::EMUSTATION) {
        QTextStream out(&file);
        out << "<gameList>\n";
        const auto esDate = [](const QString &iso) -> QString {
            const QString d = iso.trimmed().remove(QLatin1Char('-'));
            if (d.length() >= 8)
                return d.left(8) + QStringLiteral("T000000");
            if (d.length() == 4)
                return d + QStringLiteral("0101T000000");
            return QString();
        };
        for (int i = 0; i < rows.size(); ++i) {
            const auto &row = rows.at(i);
            const QString name
                = (!row.match.gameTitle.isEmpty() ? row.match.gameTitle : row.file.filename).toHtmlEscaped();
            out << "  <game>\n";
            out << "    <path>" << row.file.currentPath.toHtmlEscaped() << "</path>\n";
            out << "    <name>" << name << "</name>\n";
            out << "    <desc>" << row.match.description.toHtmlEscaped() << "</desc>\n";
            out << "    <genre>" << row.match.genre.toHtmlEscaped() << "</genre>\n";
            out << "    <players>" << row.match.players << "</players>\n";
            out << "    <region>" << row.match.region.toHtmlEscaped() << "</region>\n";
            if (!row.match.publisher.isEmpty())
                out << "    <publisher>" << row.match.publisher.toHtmlEscaped() << "</publisher>\n";
            const QString esd = esDate(row.match.releaseDate);
            if (!esd.isEmpty())
                out << "    <releasedate>" << esd << "</releasedate>\n";
            out << "  </game>\n";
            reportProgress(i + 1);
        }
        out << "</gameList>\n";
    } else if (normalized == Formats::LAUNCHBOX) {
        QTextStream out(&file);
        out << "<LaunchBox>\n";
        for (int i = 0; i < rows.size(); ++i) {
            const auto &row = rows.at(i);
            const QString title
                = (!row.match.gameTitle.isEmpty() ? row.match.gameTitle : row.file.filename).toHtmlEscaped();
            out << "  <Game>\n";
            out << "    <Title>" << title << "</Title>\n";
            out << "    <ApplicationPath>" << row.file.currentPath.toHtmlEscaped() << "</ApplicationPath>\n";
            out << "    <Region>" << row.match.region.toHtmlEscaped() << "</Region>\n";
            out << "    <Genre>" << row.match.genre.toHtmlEscaped() << "</Genre>\n";
            out << "  </Game>\n";
            reportProgress(i + 1);
        }
        out << "</LaunchBox>\n";
    } else if (normalized == Formats::CSV) {
        QTextStream out(&file);
        const auto csvField = [](const QString &s) -> QString {
            if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n'))) {
                return QLatin1Char('"') + QString(s).replace(QLatin1Char('"'), QStringLiteral("\"\""))
                    + QLatin1Char('"');
            }
            return s;
        };
        out << "file_id,title,system,path,region,confidence\n";
        for (int i = 0; i < rows.size(); ++i) {
            const auto &row = rows.at(i);
            out << row.file.id << ","
                << csvField(!row.match.gameTitle.isEmpty() ? row.match.gameTitle : row.file.filename) << ","
                << csvField(db.getSystemDisplayName(row.file.systemId)) << "," << csvField(row.file.currentPath) << ","
                << csvField(row.match.region) << "," << row.match.confidence << "\n";
            reportProgress(i + 1);
        }
    } else if (normalized == Formats::JSON) {
        QJsonArray arr;
        for (int i = 0; i < rows.size(); ++i) {
            const auto &row = rows.at(i);
            QJsonObject obj;
            obj["fileId"] = row.file.id;
            obj["title"] = row.match.gameTitle;
            obj["system"] = db.getSystemDisplayName(row.file.systemId);
            obj["path"] = row.file.currentPath;
            obj["region"] = row.match.region;
            obj["confidence"] = row.match.confidence;
            arr.append(obj);
            reportProgress(i + 1);
        }
        file.write(QJsonDocument(arr).toJson());
    } else {
        if (error)
            *error = QStringLiteral("Unknown export format: %1").arg(format);
        return false;
    }

    return true;
}

} // namespace Remus
