#pragma once

#include <QObject>
#include <QVariantList>
#include <memory>

#include "../../services/mod_catalog_provider.h"
#include "../../services/mod_workflow_service.h"
#include "../../services/patch_service.h"

namespace Remus {

class AppController;
class ModListModel;

class ModController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loadingCatalog READ isLoadingCatalog NOTIFY loadingCatalogChanged)
    Q_PROPERTY(bool installing READ isInstalling NOTIFY installingChanged)
    Q_PROPERTY(QString catalogUrl READ catalogUrl WRITE setCatalogUrl NOTIFY catalogUrlChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QVariantList installedMods READ installedMods NOTIFY installedModsChanged)

public:
    explicit ModController(AppController *appController, QObject *parent = nullptr);

    bool isLoadingCatalog() const {
        return m_loadingCatalog;
    }
    bool isInstalling() const {
        return m_installing;
    }
    QString catalogUrl() const {
        return m_catalogUrl;
    }
    QString lastError() const {
        return m_lastError;
    }
    QVariantList installedMods() const {
        return m_installedMods;
    }

    void setModel(ModListModel *model) {
        m_model = model;
    }

    Q_INVOKABLE void loadCatalog(const QString &url = QString(), bool forceRefresh = false);
    Q_INVOKABLE void loadForSelectedFile();
    Q_INVOKABLE bool installMod(const QString &modId, const QString &outputDir);
    Q_INVOKABLE bool uninstallInstallation(int installationId);

public slots:
    void setCatalogUrl(const QString &value);

signals:
    void loadingCatalogChanged();
    void installingChanged();
    void catalogUrlChanged();
    void lastErrorChanged();
    void installedModsChanged();
    void libraryChanged();

private:
    void applyToolPaths();
    void refreshInstalledMods();
    void setLastError(const QString &message);

    AppController *m_appController;
    ModCatalogProvider m_provider;
    PatchService m_patchService;
    std::unique_ptr<ModWorkflowService> m_workflow;
    ModListModel *m_model = nullptr;
    bool m_loadingCatalog = false;
    bool m_installing = false;
    QString m_catalogUrl;
    QString m_lastError;
    QVariantList m_installedMods;
};

} // namespace Remus