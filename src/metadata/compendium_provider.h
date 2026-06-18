#pragma once

#include "metadata_provider.h"
#include "compendium_normalizer.h"
#include "compendium_disc_set_types.h"
#include "multi_signal_types.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

namespace Remus {

class CompendiumProvider : public MetadataProvider {
    Q_OBJECT

public:
    explicit CompendiumProvider(QObject *parent = nullptr);
    ~CompendiumProvider() override;

    bool openDatabase(const QString &databasePath);

    QList<SearchResult> searchByName(
        const QString &title, const QString &system, const QString &region = QString()) override;
    GameMetadata getByHash(const QString &hash, const QString &system) override;
    /// Hash lookup with optional catalog size corroboration (0 = skip size check).
    GameMetadata getByHash(const QString &hash, const QString &system, qint64 fileSize);
    GameMetadata getBySerial(const QString &serial, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

    /**
     * @brief Offline multi-signal matching against compendium catalog data.
     *
     * Combines hash, filename, size, and serial corroboration with confidence scoring.
     * Used when definitive hash lookup misses (partial dumps, renamed files).
     */
    QList<CompendiumMultiSignalMatch> matchROM(const ROMSignals &input, const QString &system) const;

    /**
     * @brief Convert a multi-signal match into provider metadata.
     */
    GameMetadata metadataFromMatch(const CompendiumMultiSignalMatch &match, const QString &system) const;

    /**
     * @brief All disc sets linked to a compendium @c game_id (ordered by set_key, disc_number).
     */
    QList<CompendiumDiscSet> getDiscSetsForGame(const QString &gameId) const;

    /**
     * @brief All disc sets sharing a canonical @c set_key (library bridge).
     */
    QList<CompendiumDiscSet> getDiscSetsBySetKey(const QString &setKey) const;

    QString name() const override {
        return QStringLiteral("Compendium");
    }
    bool requiresAuth() const override {
        return false;
    }
    bool isAvailable() override;

private:
    QSqlDatabase database() const;
    int resolveSystemId(const QString &system) const;
    GameMetadata fetchGameMetadata(const QString &gameId) const;
    QMap<QString, QString> loadResolvedFacts(const QString &gameId) const;
    void populateExternalIds(GameMetadata &metadata, const QString &gameId, const QString &igdbId = QString(),
        const QString &raGameId = QString()) const;
    void closeConnection();
    void ensureFts5Index();

    static QString detectHashType(const QString &hash, QString &normalizedValue);
    GameMetadata lookupPatchByHash(const QString &hashType, const QString &normalizedHash, const QString &system,
        int systemId, qint64 fileSize = 0) const;
    void populateDiscContextFromSourceEntry(GameMetadata &metadata, const QString &sourceEntryKey) const;
    bool lookupDiscSetBySourceEntry(const QString &sourceEntryKey, CompendiumDiscSet &discSet) const;
    QList<CompendiumDiscSet> queryDiscSets(const QString &whereSql, const QVariantList &bindValues) const;
    void applyDiscContextToMatch(const ROMSignals &input, CompendiumMultiSignalMatch &match) const;

    QString m_connectionName;
    QString m_databasePath;
    Compendium::CompendiumNormalizer m_normalizer;
};

} // namespace Remus