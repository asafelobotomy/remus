#include "match_screen.h"
#include "app.h"

#include "../core/database.h"
#include "../metadata/provider_orchestrator.h"
#include "../metadata/hasheous_provider.h"
#include "../metadata/thegamesdb_provider.h"
#include "../metadata/igdb_provider.h"
#include "../core/constants/constants.h"

#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <algorithm>
#include <cctype>

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

void MatchScreen::loadFromDatabase()
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

    // Build flat entry list (without headers yet)
    std::vector<FileEntry> confident, possible, noMatch;

    for (auto it = groups.begin(); it != groups.end(); ++it) {
        Group &g = it.value();
        sortExtensions(g.extensions);

        const FileRecord &fr = g.primary;
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
        e.system = db.getSystemDisplayName(fr.systemId).toStdString();
        if (e.system.empty()) e.system = "Unknown";

        auto matchIt = allMatches.find(fr.id);
        if (matchIt != allMatches.end()) {
            const auto &mr = matchIt.value();
            e.confidence = static_cast<int>(mr.confidence);
            e.matchMethod = mr.matchMethod.toStdString();
            e.title = mr.gameTitle.toStdString();
            e.developer = mr.developer.toStdString();
            e.publisher = mr.publisher.toStdString();
            e.description = mr.description.toStdString();
            e.region = mr.region.toStdString();
            e.confirmStatus = mr.isConfirmed ? ConfirmStatus::Confirmed
                            : mr.isRejected  ? ConfirmStatus::Rejected
                            :                  ConfirmStatus::Pending;

            if (e.confidence >= static_cast<int>(Remus::Constants::Confidence::Thresholds::HIGH)) {
                e.section = Section::Confident;
                e.matchStatus = "match ✓";
                confident.push_back(std::move(e));
            } else if (e.confidence > 0) {
                e.section = Section::Possible;
                e.matchStatus = "match ?";
                possible.push_back(std::move(e));
            } else {
                e.section = Section::NoMatch;
                e.matchStatus = "no match";
                e.isPossiblyPatched = looksPatched(e.filename);
                noMatch.push_back(std::move(e));
            }
        } else {
            e.section = Section::NoMatch;
            e.matchStatus = fr.hashCalculated ? "unmatched" : "not hashed";
            e.isPossiblyPatched = looksPatched(e.filename);
            noMatch.push_back(std::move(e));
        }
    }

    // Sort each bucket alphabetically by filename
    auto byName = [](const FileEntry &a, const FileEntry &b) { return a.filename < b.filename; };
    std::sort(confident.begin(), confident.end(), byName);
    std::sort(possible.begin(), possible.end(), byName);
    std::sort(noMatch.begin(), noMatch.end(), byName);

    // Assemble with section-header rows
    std::vector<FileEntry> entries;
    auto addSection = [&](const std::vector<FileEntry> &bucket, const char *label, Section sec) {
        if (bucket.empty()) return;
        FileEntry hdr;
        hdr.isHeader = true;
        hdr.section = sec;
        hdr.filename = label;
        entries.push_back(hdr);
        for (const auto &e : bucket)
            entries.push_back(e);
    };

    addSection(confident, "── Confident Match (≥90%) ────────────────────────────", Section::Confident);
    addSection(possible,  "── Possible Match ────────────────────────────────────", Section::Possible);
    addSection(noMatch,   "── No Match ──────────────────────────────────────────", Section::NoMatch);

    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        m_files = std::move(entries);
    }

    m_fileList.setCount(static_cast<int>(m_files.size()));
    // Position on first non-header entry
    if (!m_files.empty()) {
        for (int i = 0; i < static_cast<int>(m_files.size()); ++i) {
            if (!m_files[static_cast<size_t>(i)].isHeader) {
                m_fileList.setSelected(i);
                break;
            }
        }
    }

    // Default focus based on content
    m_focus = m_files.empty() ? Focus::PathInput : Focus::FileList;
}

void MatchScreen::forceRefresh()
{
    loadFromDatabase();
}

// ════════════════════════════════════════════════════════════
// Metadata enrichment
// ════════════════════════════════════════════════════════════

Remus::ProviderOrchestrator &MatchScreen::getOrchestrator()
{
    if (!m_orchestrator) {
        using namespace Remus;
        m_orchestrator = std::make_unique<ProviderOrchestrator>();

        // Hasheous — free, no auth
        auto *hasheous = new HasheousProvider();
        const auto hInfo = Constants::Providers::getProviderInfo(Constants::Providers::HASHEOUS);
        m_orchestrator->addProvider(Constants::Providers::HASHEOUS, hasheous, hInfo->priority);

        // TheGamesDB — free, optional API key
        QSettings settings;
        auto *tgdb = new TheGamesDBProvider();
        QString tgdbKey = settings.value(Constants::Settings::Providers::THEGAMESDB_API_KEY).toString();
        if (!tgdbKey.isEmpty()) tgdb->setApiKey(tgdbKey);
        const auto tInfo = Constants::Providers::getProviderInfo(Constants::Providers::THEGAMESDB);
        m_orchestrator->addProvider(Constants::Providers::THEGAMESDB, tgdb, tInfo->priority);

        // IGDB — requires Twitch credentials
        auto *igdb = new IGDBProvider();
        QString igdbId  = settings.value(Constants::Settings::Providers::IGDB_CLIENT_ID).toString();
        QString igdbSec = settings.value(Constants::Settings::Providers::IGDB_CLIENT_SECRET).toString();
        if (!igdbId.isEmpty() && !igdbSec.isEmpty()) {
            igdb->setCredentials(igdbId, igdbSec);
            const auto iInfo = Constants::Providers::getProviderInfo(Constants::Providers::IGDB);
            m_orchestrator->addProvider(Constants::Providers::IGDB, igdb, iInfo->priority);
        } else {
            delete igdb;
        }
    }
    return *m_orchestrator;
}

void MatchScreen::enrichSelectedMetadata()
{
    if (m_enrichTask.running()) {
        m_app.toast("Enrichment already in progress", Toast::Level::Warning);
        return;
    }

    int sel = m_fileList.selected();
    int targetFileId = 0;
    std::string targetTitle;
    std::string targetSystem;
    std::string targetHash;
    int gameId = 0;
    {
        std::lock_guard<std::mutex> lock(m_filesMutex);
        if (sel < 0 || sel >= static_cast<int>(m_files.size())) return;
        const FileEntry &e = m_files[static_cast<size_t>(sel)];
        if (e.isHeader || e.fileId == 0) return;
        if (e.section == Section::NoMatch) {
            m_app.toast("No match to enrich — confirm or manual-match first", Toast::Level::Warning);
            return;
        }
        targetFileId = e.fileId;
        targetTitle  = e.title;
        targetSystem = e.system;
        targetHash   = e.hash;
    }

    // Look up gameId from DB
    auto mr = m_app.db().getMatchForFile(targetFileId);
    if (mr.gameId == 0) {
        m_app.toast("No game match found in database", Toast::Level::Warning);
        return;
    }
    gameId = mr.gameId;

    m_app.toast("Enriching metadata…", Toast::Level::Info, 1500);

    m_enrichTask.start([this, targetFileId, targetTitle, targetSystem, targetHash, gameId]() {
        auto &orch = getOrchestrator();
        auto meta = orch.searchWithFallback(
            QString::fromStdString(targetHash),
            QString::fromStdString(targetTitle),
            QString::fromStdString(targetSystem));

        if (meta.title.isEmpty()) {
            m_app.post([this]() {
                m_app.toast("No additional metadata found", Toast::Level::Warning);
            });
            return;
        }

        // Update DB on main thread
        m_app.post([this, meta, gameId, targetFileId]() {
            m_app.db().updateGame(
                gameId,
                meta.publisher,
                meta.developer,
                meta.releaseDate,
                meta.description,
                meta.genres.join(", "),
                meta.players > 0 ? QString::number(meta.players) : QString(),
                meta.rating);

            // Refresh display entry with new metadata
            {
                std::lock_guard<std::mutex> lock(m_filesMutex);
                for (auto &f : m_files) {
                    if (f.fileId == targetFileId) {
                        if (!meta.title.isEmpty())
                            f.title = meta.title.toStdString();
                        if (!meta.developer.isEmpty())
                            f.developer = meta.developer.toStdString();
                        if (!meta.publisher.isEmpty())
                            f.publisher = meta.publisher.toStdString();
                        if (!meta.description.isEmpty())
                            f.description = meta.description.toStdString();
                        if (!meta.region.isEmpty())
                            f.region = meta.region.toStdString();
                        break;
                    }
                }
            }

            m_app.toast("Metadata enriched from " + meta.providerId.toStdString(),
                        Toast::Level::Success);
        });
    });
}

/*static*/ bool MatchScreen::looksPatched(const std::string &filename)
{
    // Keyword heuristics for patched/translated/hacked ROMs
    static const std::vector<std::string> keywords = {
        "(patch", "(patched", "(translated", "(translation",
        "(hack", "(hacked", "(mod", "(modified",
        "[t]", "[h]", "[t-", "(t)"
    };
    std::string lower = filename;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    for (const auto &kw : keywords) {
        if (lower.find(kw) != std::string::npos)
            return true;
    }
    return false;
}
