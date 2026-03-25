#include "library_screen.h"
#include "app.h"

#include "../core/database.h"
#include "../core/constants/confidence.h"
#include "../core/constants/files.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <map>
#include <set>

using Remus::Database;
using Remus::FileRecord;

namespace {

QString baseNameForGrouping(const QString &filename)
{
    QFileInfo fi(filename);
    QString baseName = fi.completeBaseName();

    static QRegularExpression trackPattern(
        R"(\s*\(Track\s*\d+\)$)", QRegularExpression::CaseInsensitiveOption);
    baseName.remove(trackPattern);

    return baseName.trimmed();
}

QString groupKey(const FileRecord &file)
{
    QFileInfo info(file.originalPath);
    return info.path() + "/" + baseNameForGrouping(file.filename);
}

void sortExtensions(QStringList &exts)
{
    std::sort(exts.begin(), exts.end(), [](const QString &a, const QString &b) {
        const int pa = Remus::Constants::Files::displayPriority(a);
        const int pb = Remus::Constants::Files::displayPriority(b);
        if (pa != pb) return pa < pb;
        return a < b;
    });
}

bool preferPrimaryCandidate(const FileRecord &candidate, const FileRecord &current,
                            const QMap<int, Database::MatchResult> &matches)
{
    const bool candidateHasMatch = matches.contains(candidate.id);
    const bool currentHasMatch = matches.contains(current.id);
    if (candidateHasMatch != currentHasMatch) {
        return candidateHasMatch;
    }

    auto isPrimaryExtension = [](const QString &ext) {
        return Remus::Constants::Files::isPrimaryDiscExtension(ext);
    };

    if (candidate.isPrimary != current.isPrimary) {
        return candidate.isPrimary;
    }

    if (isPrimaryExtension(candidate.extension) && !isPrimaryExtension(current.extension)) {
        return true;
    }

    return false;
}

} // namespace

// ════════════════════════════════════════════════════════════
// Data loading
// ════════════════════════════════════════════════════════════

void LibraryScreen::loadFromDatabase()
{
    auto &db = m_app.db();
    auto allFiles = db.getExistingFiles();
    auto allMatches = db.getAllMatches();

    struct Group {
        FileRecord primary;
        bool hasPrimary = false;
        QStringList extensions;
    };

    QMap<QString, Group> groups;

    for (const auto &fr : allFiles) {
        const QString key = groupKey(fr);
        Group &g = groups[key];

        const QString ext = fr.extension.toLower();
        if (!g.extensions.contains(ext)) {
            g.extensions.append(ext);
        }

        if (!g.hasPrimary || preferPrimaryCandidate(fr, g.primary, allMatches)) {
            g.primary = fr;
            g.hasPrimary = true;
        }
    }

    // Build flat list of grouped entries by system (no filtering)
    std::map<std::string, std::vector<FileEntry>> bySystem;

    for (auto it = groups.begin(); it != groups.end(); ++it) {
        Group &g = it.value();
        sortExtensions(g.extensions);

        const FileRecord &fr = g.primary;
        std::string system = db.getSystemDisplayName(fr.systemId).toStdString();
        if (system.empty()) system = "Unknown";

        FileEntry e;
        e.fileId = fr.id;
        const std::string baseName = baseNameForGrouping(fr.filename).toStdString();
        const std::string extDisplay = g.extensions.join(" ").toStdString();
        if (g.extensions.size() > 1) {
            e.filename = baseName + " [" + extDisplay + "]";
        } else {
            e.filename = fr.filename.toStdString();
        }
        e.extensions = extDisplay;
        e.hash = fr.crc32.toStdString();
        e.system = system;
        e.path = fr.currentPath.toStdString();

        auto matchIt = allMatches.find(fr.id);
        if (matchIt != allMatches.end()) {
            e.confidence = static_cast<int>(matchIt.value().confidence);
            e.matchMethod = matchIt.value().matchMethod.toStdString();
            e.title = matchIt.value().gameTitle.toStdString();
            e.developer = matchIt.value().developer.toStdString();
            e.publisher = matchIt.value().publisher.toStdString();
            e.description = matchIt.value().description.toStdString();
            e.region = matchIt.value().region.toStdString();

            if (matchIt.value().isConfirmed)
                e.confirmStatus = ConfirmationStatus::Confirmed;
            else if (matchIt.value().isRejected)
                e.confirmStatus = ConfirmationStatus::Rejected;
            else
                e.confirmStatus = ConfirmationStatus::Pending;

            if (e.confidence >= static_cast<int>(Remus::Constants::Confidence::Thresholds::HIGH)) e.matchStatus = "match ✓";
            else if (e.confidence > 0) e.matchStatus = "match ?";
            else e.matchStatus = "no match";
        } else {
            e.matchStatus = fr.hashCalculated ? "unmatched" : "pending";
        }

        bySystem[system].push_back(std::move(e));
    }

    // Build flat list with headers
    std::vector<FileEntry> flat;
    for (const auto &[system, files] : bySystem) {
        FileEntry header;
        header.isHeader = true;
        header.system = system + " (" + std::to_string(files.size()) + ")";
        flat.push_back(std::move(header));

        for (const auto &f : files)
            flat.push_back(f);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_allEntries = std::move(flat);
    }

    applyFilter();
}

void LibraryScreen::applyFilter()
{
    std::string filterLower = m_filterInput.value();
    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

    std::vector<FileEntry> filtered;
    std::set<std::string> systemSet;
    int totalFiles = 0;
    int totalMatched = 0;
    std::string currentSystem;
    std::vector<FileEntry> currentGroup;

    auto flushGroup = [&]() {
        if (!currentGroup.empty()) {
            FileEntry hdr;
            hdr.isHeader = true;
            hdr.system = currentSystem + " (" + std::to_string(currentGroup.size()) + ")";
            filtered.push_back(std::move(hdr));
            for (auto &f : currentGroup)
                filtered.push_back(std::move(f));
            currentGroup.clear();
        }
    };

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto &e : m_allEntries) {
        if (e.isHeader) {
            flushGroup();
            // Extract raw system name (strip count suffix)
            currentSystem = e.system;
            auto paren = currentSystem.rfind(" (");
            if (paren != std::string::npos)
                currentSystem = currentSystem.substr(0, paren);
            continue;
        }

        totalFiles++;
        if (e.confidence > 0) totalMatched++;

        if (!filterLower.empty()) {
            std::string sysLower = currentSystem;
            std::transform(sysLower.begin(), sysLower.end(), sysLower.begin(), ::tolower);
            std::string fnameLower = e.filename;
            std::transform(fnameLower.begin(), fnameLower.end(), fnameLower.begin(), ::tolower);
            std::string titleLower = e.title;
            std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
            if (sysLower.find(filterLower) == std::string::npos &&
                fnameLower.find(filterLower) == std::string::npos &&
                titleLower.find(filterLower) == std::string::npos)
                continue;
        }

        systemSet.insert(currentSystem);
        currentGroup.push_back(e);
    }
    flushGroup();

    m_entries = std::move(filtered);
    m_totalFiles = totalFiles;
    m_totalSystems = static_cast<int>(systemSet.size());
    m_totalMatched = totalMatched;
    m_fileList.setCount(static_cast<int>(m_entries.size()));
    if (!m_entries.empty() && m_fileList.selected() < 0)
        m_fileList.setSelected(0);
}

void LibraryScreen::forceRefresh()
{
    loadFromDatabase();
}
