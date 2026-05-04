#pragma once

#include <QObject>
#include <memory>

#include "../../core/rom_bundler.h"

namespace Remus {

class AppController;

class ExportController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exporting READ isExporting NOTIFY exportingChanged)
    Q_PROPERTY(int bundledFiles READ bundledFiles NOTIFY bundleProgressChanged)
    Q_PROPERTY(int totalBundleFiles READ totalBundleFiles NOTIFY bundleProgressChanged)
    Q_PROPERTY(QString progressMessage READ progressMessage NOTIFY progressMessageChanged)
    Q_PROPERTY(QString lastOutputPath READ lastOutputPath NOTIFY exportFinished)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)

public:
    explicit ExportController(AppController *appController, QObject *parent = nullptr);

    bool isExporting() const { return m_exporting; }
    int bundledFiles() const { return m_bundledFiles; }
    int totalBundleFiles() const { return m_totalBundleFiles; }
    QString progressMessage() const { return m_progressMessage; }
    QString lastOutputPath() const { return m_lastOutputPath; }
    QString lastMessage() const { return m_lastMessage; }

    Q_INVOKABLE void bundleSelected(const QString &scanDir, const QString &namingTemplate = QString());
    Q_INVOKABLE void bundleAll(const QString &scanDir, const QString &namingTemplate = QString());
    Q_INVOKABLE bool exportM3u(const QString &outputPath);

signals:
    void exportingChanged();
    void bundleProgressChanged();
    void progressMessageChanged();
    void exportFinished();
    void lastMessageChanged();
    void libraryChanged();

private:
    GameMetadata metadataForMatch(const Database::MatchResult &match) const;
    void setLastMessage(const QString &message);

    AppController *m_appController;
    std::unique_ptr<RomBundler> m_bundler;
    bool m_exporting = false;
    int m_bundledFiles = 0;
    int m_totalBundleFiles = 0;
    QString m_progressMessage;
    QString m_lastOutputPath;
    QString m_lastMessage;
};

} // namespace Remus