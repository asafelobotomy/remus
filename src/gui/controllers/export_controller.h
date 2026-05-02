#pragma once

#include <QObject>
#include <memory>

#include "../../core/rom_bundler.h"

namespace Remus {

class AppController;

class ExportController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool exporting READ isExporting NOTIFY exportingChanged)
    Q_PROPERTY(QString lastOutputPath READ lastOutputPath NOTIFY exportFinished)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)

public:
    explicit ExportController(AppController *appController, QObject *parent = nullptr);

    bool isExporting() const { return m_exporting; }
    QString lastOutputPath() const { return m_lastOutputPath; }
    QString lastMessage() const { return m_lastMessage; }

    Q_INVOKABLE void bundleSelected(const QString &destinationDir);
    Q_INVOKABLE void bundleAll(const QString &destinationDir);
    Q_INVOKABLE bool exportM3u(const QString &outputPath);

signals:
    void exportingChanged();
    void exportFinished();
    void lastMessageChanged();
    void libraryChanged();

private:
    GameMetadata metadataForMatch(const Database::MatchResult &match) const;
    void setLastMessage(const QString &message);

    AppController *m_appController;
    std::unique_ptr<RomBundler> m_bundler;
    bool m_exporting = false;
    QString m_lastOutputPath;
    QString m_lastMessage;
};

} // namespace Remus