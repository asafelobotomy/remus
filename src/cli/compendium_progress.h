#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>

/**
 * @brief Machine-readable progress sidecar (<db>.progress.json) and human-readable CLI lines.
 *
 * Poll with: jq . data/compendium/remus_compendium.db.progress.json
 * Or:        watch -n5 'jq
 * "{pct:.overall_pct,phase:.build_phase,pass:.enrichment_pass_name,done:.enrichment_done,total:.enrichment_total,detail:.enrichment_detail}"'
 * ...
 */
class CompendiumProgressWriter {
public:
    explicit CompendiumProgressWriter(const QString &progressPath);

    bool isActive() const {
        return !m_progressPath.isEmpty();
    }
    const QString &progressPath() const {
        return m_progressPath;
    }

    void setStartedAt(const QDateTime &startedAt);
    void setElapsedMs(qint64 elapsedMs);

    void writeBuildProgress(const QString &status, int current, int total, const QString &currentSource, int overallPct,
        const QString &buildPhase, const QJsonObject &extra = { });

    void writeEnrichmentPassStart(int passIdx, int totalPasses, const QString &passName, const QString &sourceKey);
    void writeEnrichmentPassProgress(const QString &sourceKey, const QString &phase, int done, int total,
        const QString &detail, int gamesEnriched = -1, int factsInserted = -1);

    void writeTerminal(const QString &status, qint64 elapsedMs, const QString &buildPhase = QString());

    /// Generic long-running task progress (consolidate, FTS, etc.).
    void writeTaskProgress(
        const QString &taskName, const QString &phase, int done, int total, const QString &detail, int overallPct = -1);

    /// Immediate, line-buffered CLI progress (safe for long-running passes).
    static void logProgressLine(const QString &line);

private:
    void writeObject(const QJsonObject &obj);
    int enrichmentOverallPct(int passIdx, int totalPasses, int done, int total) const;

    QString m_progressPath;
    QDateTime m_startedAt;
    qint64 m_elapsedMs = 0;
    int m_passIdx = 0;
    int m_totalPasses = 1;
    QString m_currentPassName;
    QString m_currentSourceKey;
};

/**
 * @brief Active enrichment pass scope — enrichers report in-pass progress without signature churn.
 *
 * Created by runCompendiumEnrichmentPasses() before each pass; cleared after the pass returns.
 */
class CompendiumEnrichmentProgressScope {
public:
    CompendiumEnrichmentProgressScope(
        CompendiumProgressWriter *writer, const QString &sourceKey, int passIdx, int totalPasses);
    ~CompendiumEnrichmentProgressScope();

    static CompendiumEnrichmentProgressScope *active();

    void report(const QString &phase, int done, int total, const QString &detail = { }, int gamesEnriched = -1,
        int factsInserted = -1) const;

private:
    CompendiumProgressWriter *m_writer = nullptr;
    QString m_sourceKey;
    int m_passIdx = 0;
    int m_totalPasses = 1;
};

/// Convenience for enrichers — no-op when no progress scope is active.
void reportCompendiumEnrichmentProgress(const QString &phase, int done, int total, const QString &detail = { },
    int gamesEnriched = -1, int factsInserted = -1);
