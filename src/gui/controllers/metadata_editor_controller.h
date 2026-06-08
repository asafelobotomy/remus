#pragma once

#include <QObject>
#include <QVariantMap>

namespace Remus {

class AppController;

class MetadataEditorController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap currentGame READ currentGame NOTIFY currentGameChanged)
    Q_PROPERTY(bool dirty READ isDirty NOTIFY dirtyChanged)

public:
    explicit MetadataEditorController(AppController *appController, QObject *parent = nullptr);

    QVariantMap currentGame() const {
        return m_currentGame;
    }
    bool isDirty() const {
        return m_dirty;
    }

    Q_INVOKABLE void loadForSelectedFile();
    Q_INVOKABLE void load(int gameId);
    Q_INVOKABLE void setField(const QString &field, const QVariant &value);
    Q_INVOKABLE bool save();
    Q_INVOKABLE void discard();

signals:
    void currentGameChanged();
    void dirtyChanged();
    void saveFailed(const QString &message);
    void gameSaved();
    void libraryChanged();

private:
    QVariantMap buildGameMap(int gameId) const;
    void setDirty(bool dirty);

    AppController *m_appController;
    int m_currentGameId = 0;
    QVariantMap m_currentGame;
    QVariantMap m_pendingFields;
    bool m_dirty = false;
};

} // namespace Remus