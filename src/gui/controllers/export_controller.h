#pragma once

#include <QObject>
#include <memory>

#include "../../core/rom_bundler.h"
#include "../../core/m3u_generator.h"

namespace Remus {

class AppController;

/**
 * @brief Handles two distinct export responsibilities:
 *
 *  1. **Pipeline bundling** (stage 5 of runAll): `bundleSelected` / `bundleAll`
 *     copy matched files into a structured Remus Library directory tree and
 *     mark them `is_bundled` in the database.
 *
 *  2. **Frontend export** (utilities panel): `exportFrontend` / `exportPreview`
 *     / `generateM3uPlaylists` produce RetroArch, ES-DE, CSV, or M3U outputs
 *     from the confirmed library for consumption by external frontends.
 *
 * These two pathways share only the `AppController` dependency; all signals,
 * properties, and methods below belong to exactly one of the two roles.
 */
class ExportController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exporting READ isExporting NOTIFY exportingChanged)
    Q_PROPERTY(int bundledFiles READ bundledFiles NOTIFY bundleProgressChanged)
    Q_PROPERTY(int totalBundleFiles READ totalBundleFiles NOTIFY bundleProgressChanged)
    Q_PROPERTY(int exportProgress READ exportProgress NOTIFY exportProgressChanged)
    Q_PROPERTY(int exportTotal READ exportTotal NOTIFY exportTotalChanged)
    Q_PROPERTY(QString progressMessage READ progressMessage NOTIFY progressMessageChanged)
    Q_PROPERTY(QString lastOutputPath READ lastOutputPath NOTIFY exportFinished)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)

public:
    explicit ExportController(AppController *appController, QObject *parent = nullptr);

    bool isExporting() const {
        return m_exporting;
    }
    int bundledFiles() const {
        return m_bundledFiles;
    }
    int totalBundleFiles() const {
        return m_totalBundleFiles;
    }
    int exportProgress() const {
        return m_exportProgress;
    }
    int exportTotal() const {
        return m_exportTotal;
    }
    QString progressMessage() const {
        return m_progressMessage;
    }
    QString lastOutputPath() const {
        return m_lastOutputPath;
    }
    QString lastMessage() const {
        return m_lastMessage;
    }

    Q_INVOKABLE void bundleSelected(const QString &scanDir, const QString &namingTemplate = QString());
    Q_INVOKABLE void bundleAll(const QString &scanDir, const QString &namingTemplate = QString());
    Q_INVOKABLE int generateM3uPlaylists(const QString &outputDir, const QString &systemsCsv = QString());

    Q_INVOKABLE QVariantMap exportPreview(const QString &systemsCsv = QString());
    Q_INVOKABLE bool exportFrontend(
        const QString &format, const QString &outputPath, const QString &systemsCsv = QString());

signals:
    void exportingChanged();
    void bundleProgressChanged();
    void exportProgressChanged();
    void exportTotalChanged();
    void progressMessageChanged();
    void exportFinished();
    void lastMessageChanged();
    void libraryChanged();

private:
    GameMetadata metadataForMatch(const Database::MatchResult &match) const;
    void setLastMessage(const QString &message);
    QStringList parseSystemsFilter(const QString &systemsCsv) const;

    AppController *m_appController;
    std::unique_ptr<RomBundler> m_bundler;
    bool m_exporting = false;
    int m_bundledFiles = 0;
    int m_totalBundleFiles = 0;
    int m_exportProgress = 0;
    int m_exportTotal = 0;
    QString m_progressMessage;
    QString m_lastOutputPath;
    QString m_lastMessage;
};

} // namespace Remus
