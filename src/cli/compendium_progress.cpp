#include "compendium_progress.h"

#include <QFile>
#include <QJsonDocument>
#include <QTextStream>
#include <cstdio>

namespace {

CompendiumEnrichmentProgressScope *g_activeScope = nullptr;

QString formatFraction(int done, int total) {
    if (total > 0)
        return QStringLiteral("%1/%2").arg(done).arg(total);
    if (done > 0)
        return QString::number(done);
    return QStringLiteral("—");
}

QString formatPercent(int done, int total) {
    if (total <= 0)
        return QStringLiteral("—");
    return QStringLiteral("%1%").arg((done * 100) / total);
}

} // namespace

CompendiumProgressWriter::CompendiumProgressWriter(const QString &progressPath)
    : m_progressPath(progressPath) { }

void CompendiumProgressWriter::setStartedAt(const QDateTime &startedAt) {
    m_startedAt = startedAt;
}

void CompendiumProgressWriter::setElapsedMs(qint64 elapsedMs) {
    m_elapsedMs = elapsedMs;
}

void CompendiumProgressWriter::logProgressLine(const QString &line) {
    QTextStream out(stdout);
    out << line << Qt::endl;
    out.flush();
    std::fflush(stdout);
}

void CompendiumProgressWriter::writeObject(const QJsonObject &obj) {
    if (m_progressPath.isEmpty())
        return;
    QFile progressFile(m_progressPath);
    if (!progressFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;
    progressFile.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    progressFile.flush();
}

int CompendiumProgressWriter::enrichmentOverallPct(int passIdx, int totalPasses, int done, int total) const {
    if (totalPasses <= 0)
        return 10;
    const int passBand = 85 / totalPasses;
    const int passStart = 10 + (passIdx - 1) * passBand;
    if (total <= 0)
        return passStart;
    const int inPass = (done * passBand) / total;
    return std::min(95, passStart + inPass);
}

void CompendiumProgressWriter::writeBuildProgress(const QString &status, int current, int total,
    const QString &currentSource, int overallPct, const QString &buildPhase, const QJsonObject &extra) {
    QJsonObject obj {
        { QStringLiteral("status"), status },
        { QStringLiteral("current"), current },
        { QStringLiteral("total"), total },
        { QStringLiteral("current_source"), currentSource },
        { QStringLiteral("overall_pct"), overallPct },
        { QStringLiteral("elapsed_ms"), m_elapsedMs },
        { QStringLiteral("updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
    };
    if (!m_startedAt.isNull())
        obj.insert(QStringLiteral("started_at"), m_startedAt.toString(Qt::ISODate));
    if (!buildPhase.isEmpty())
        obj.insert(QStringLiteral("build_phase"), buildPhase);
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
        obj.insert(it.key(), it.value());
    writeObject(obj);
}

void CompendiumProgressWriter::writeEnrichmentPassStart(
    int passIdx, int totalPasses, const QString &passName, const QString &sourceKey) {
    m_passIdx = passIdx;
    m_totalPasses = std::max(1, totalPasses);
    m_currentPassName = passName;
    m_currentSourceKey = sourceKey;

    const int pct = enrichmentOverallPct(passIdx, m_totalPasses, 0, 1);
    writeBuildProgress(QStringLiteral("enriching"), passIdx, m_totalPasses, sourceKey, pct, QStringLiteral("enriching"),
        {
            { QStringLiteral("enrichment_pass_current"), passIdx },
            { QStringLiteral("enrichment_pass_total"), m_totalPasses },
            { QStringLiteral("enrichment_pass_name"), passName },
            { QStringLiteral("enrichment_source_key"), sourceKey },
            { QStringLiteral("enrichment_phase"), QStringLiteral("starting") },
            { QStringLiteral("enrichment_done"), 0 },
            { QStringLiteral("enrichment_total"), 0 },
            { QStringLiteral("enrichment_detail"), QString() },
        });

    logProgressLine(QStringLiteral("[ENRICH %1%] pass %2/%3 — %4 (%5)")
            .arg(pct)
            .arg(passIdx)
            .arg(m_totalPasses)
            .arg(passName, sourceKey));
}

void CompendiumProgressWriter::writeEnrichmentPassProgress(const QString &sourceKey, const QString &phase, int done,
    int total, const QString &detail, int gamesEnriched, int factsInserted) {
    const int pct = enrichmentOverallPct(m_passIdx > 0 ? m_passIdx : 1, m_totalPasses, done, total);

    QJsonObject extra {
        { QStringLiteral("enrichment_pass_current"), m_passIdx },
        { QStringLiteral("enrichment_pass_total"), m_totalPasses },
        { QStringLiteral("enrichment_pass_name"), m_currentPassName },
        { QStringLiteral("enrichment_source_key"), sourceKey },
        { QStringLiteral("enrichment_phase"), phase },
        { QStringLiteral("enrichment_done"), done },
        { QStringLiteral("enrichment_total"), total },
        { QStringLiteral("enrichment_detail"), detail },
    };
    if (gamesEnriched >= 0)
        extra.insert(QStringLiteral("enrichment_games_enriched"), gamesEnriched);
    if (factsInserted >= 0)
        extra.insert(QStringLiteral("enrichment_facts_inserted"), factsInserted);

    writeBuildProgress(
        QStringLiteral("enriching"), m_passIdx, m_totalPasses, sourceKey, pct, QStringLiteral("enriching"), extra);

    QString line
        = QStringLiteral("[ENRICH %1%] %2 — %3 %4").arg(pct).arg(sourceKey, phase, formatFraction(done, total));
    if (!detail.isEmpty())
        line += QStringLiteral(" (%1)").arg(detail);
    logProgressLine(line);
}

void CompendiumProgressWriter::writeTerminal(const QString &status, qint64 elapsedMs, const QString &buildPhase) {
    m_elapsedMs = elapsedMs;
    QJsonObject obj {
        { QStringLiteral("status"), status },
        { QStringLiteral("elapsed_ms"), elapsedMs },
        { QStringLiteral("updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate) },
        { QStringLiteral("overall_pct"), status == QStringLiteral("complete") ? 100 : 0 },
    };
    if (!buildPhase.isEmpty())
        obj.insert(QStringLiteral("build_phase"), buildPhase);
    if (!m_startedAt.isNull())
        obj.insert(QStringLiteral("started_at"), m_startedAt.toString(Qt::ISODate));
    writeObject(obj);

    if (status == QStringLiteral("complete")) {
        logProgressLine(QStringLiteral("[ENRICH] complete (%1 ms)").arg(elapsedMs));
    } else if (status == QStringLiteral("failed")) {
        logProgressLine(QStringLiteral("[ENRICH] failed after %1 ms").arg(elapsedMs));
    }
}

void CompendiumProgressWriter::writeTaskProgress(
    const QString &taskName, const QString &phase, int done, int total, const QString &detail, int overallPct) {
    const int pct = overallPct >= 0 ? overallPct : (total > 0 ? (done * 100) / total : 0);
    writeBuildProgress(QStringLiteral("in_progress"), done, total > 0 ? total : done, taskName, pct, phase,
        {
            { QStringLiteral("task_name"), taskName },
            { QStringLiteral("task_phase"), phase },
            { QStringLiteral("task_done"), done },
            { QStringLiteral("task_total"), total },
            { QStringLiteral("task_detail"), detail },
        });

    QString line = QStringLiteral("[%1 %2%] %3 — %4 %5").arg(taskName).arg(pct).arg(phase, formatFraction(done, total));
    if (!detail.isEmpty())
        line += QStringLiteral(" (%1)").arg(detail);
    logProgressLine(line);
}

CompendiumEnrichmentProgressScope::CompendiumEnrichmentProgressScope(
    CompendiumProgressWriter *writer, const QString &sourceKey, int passIdx, int totalPasses)
    : m_writer(writer)
    , m_sourceKey(sourceKey)
    , m_passIdx(passIdx)
    , m_totalPasses(totalPasses) {
    g_activeScope = this;
}

CompendiumEnrichmentProgressScope::~CompendiumEnrichmentProgressScope() {
    if (g_activeScope == this)
        g_activeScope = nullptr;
}

CompendiumEnrichmentProgressScope *CompendiumEnrichmentProgressScope::active() {
    return g_activeScope;
}

void CompendiumEnrichmentProgressScope::report(
    const QString &phase, int done, int total, const QString &detail, int gamesEnriched, int factsInserted) const {
    if (!m_writer || !m_writer->isActive())
        return;
    m_writer->writeEnrichmentPassProgress(m_sourceKey, phase, done, total, detail, gamesEnriched, factsInserted);
}

void reportCompendiumEnrichmentProgress(
    const QString &phase, int done, int total, const QString &detail, int gamesEnriched, int factsInserted) {
    if (CompendiumEnrichmentProgressScope *scope = CompendiumEnrichmentProgressScope::active())
        scope->report(phase, done, total, detail, gamesEnriched, factsInserted);
}
