#pragma once

#include <QObject>
#include <QVariantList>
#include <memory>

#include "../../core/organize_engine.h"

namespace Remus {

class AppController;

class OrganizeController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString namingTemplate READ namingTemplate WRITE setNamingTemplate NOTIFY namingTemplateChanged)
    Q_PROPERTY(bool organizing READ isOrganizing NOTIFY organizingChanged)
    Q_PROPERTY(int organizedFiles READ organizedFiles NOTIFY organizeProgressChanged)
    Q_PROPERTY(int totalOrganizeFiles READ totalOrganizeFiles NOTIFY organizeProgressChanged)
    Q_PROPERTY(QString progressMessage READ progressMessage NOTIFY progressMessageChanged)
    Q_PROPERTY(QVariantList previewEntries READ previewEntries NOTIFY previewEntriesChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit OrganizeController(AppController *appController, QObject *parent = nullptr);

    QString namingTemplate() const { return m_namingTemplate; }
    bool isOrganizing() const { return m_organizing; }
    int organizedFiles() const { return m_organizedFiles; }
    int totalOrganizeFiles() const { return m_totalOrganizeFiles; }
    QString progressMessage() const { return m_progressMessage; }
    QVariantList previewEntries() const { return m_previewEntries; }
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void previewOrganize(const QString &destinationDir);
    Q_INVOKABLE void applyOrganize(const QString &destinationDir);
    Q_INVOKABLE void organizeAll(const QString &destinationDir);
    Q_INVOKABLE void undoLast();

public slots:
    void setNamingTemplate(const QString &value);

signals:
    void namingTemplateChanged();
    void organizingChanged();
    void organizeProgressChanged();
    void progressMessageChanged();
    void previewEntriesChanged();
    void lastErrorChanged();
    void libraryChanged();

private:
    QList<int> targetFileIds() const;
    QList<int> bundledFileIds() const;
    QMap<int, GameMetadata> metadataForFiles(const QList<int> &fileIds) const;
    void setLastError(const QString &message);
    void runOrganize(const QString &destinationDir, bool dryRun, bool allBundled = false);

    AppController *m_appController;
    std::unique_ptr<OrganizeEngine> m_engine;
    QString m_namingTemplate;
    bool m_organizing = false;
    int m_organizedFiles = 0;
    int m_totalOrganizeFiles = 0;
    QString m_progressMessage;
    QVariantList m_previewEntries;
    QString m_lastError;
    int m_lastUndoId = 0;
};

} // namespace Remus