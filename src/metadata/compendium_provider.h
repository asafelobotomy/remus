#pragma once

#include "metadata_provider.h"
#include "compendium_normalizer.h"

#include <QSqlDatabase>
#include <QString>

namespace Remus {

class CompendiumProvider : public MetadataProvider
{
    Q_OBJECT

public:
    explicit CompendiumProvider(QObject *parent = nullptr);
    ~CompendiumProvider() override;

    bool openDatabase(const QString &databasePath);

    QList<SearchResult> searchByName(const QString &title,
                                     const QString &system,
                                     const QString &region = QString()) override;
    GameMetadata getByHash(const QString &hash, const QString &system) override;
    GameMetadata getBySerial(const QString &serial, const QString &system) override;
    GameMetadata getById(const QString &id) override;
    ArtworkUrls getArtwork(const QString &id) override;

    QString name() const override { return QStringLiteral("Compendium"); }
    bool requiresAuth() const override { return false; }
    bool isAvailable() override;

private:
    QSqlDatabase database() const;
    int resolveSystemId(const QString &system) const;
    GameMetadata fetchGameMetadata(const QString &gameId) const;
    QMap<QString, QString> loadResolvedFacts(const QString &gameId) const;
    void populateExternalIds(GameMetadata &metadata, const QString &gameId) const;
    void closeConnection();

    static QString detectHashType(const QString &hash, QString &normalizedValue);

    QString m_connectionName;
    QString m_databasePath;
    Compendium::CompendiumNormalizer m_normalizer;
};

} // namespace Remus