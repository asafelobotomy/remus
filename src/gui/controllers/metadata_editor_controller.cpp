#include "metadata_editor_controller.h"

#include "app_controller.h"

namespace Remus {

MetadataEditorController::MetadataEditorController(AppController *appController, QObject *parent)
    : QObject(parent)
    , m_appController(appController)
{
    connect(m_appController, &AppController::selectedGameChanged, this, &MetadataEditorController::loadForSelectedFile);
}

void MetadataEditorController::loadForSelectedFile()
{
    load(m_appController ? m_appController->selectedGameId() : 0);
}

void MetadataEditorController::load(int gameId)
{
    m_currentGameId = gameId;
    m_pendingFields.clear();
    m_currentGame = buildGameMap(gameId);
    setDirty(false);
    emit currentGameChanged();
}

void MetadataEditorController::setField(const QString &field, const QVariant &value)
{
    if (!m_currentGame.contains(field)) {
        return;
    }

    m_currentGame.insert(field, value);
    m_pendingFields.insert(field, value);
    setDirty(true);
    emit currentGameChanged();
}

bool MetadataEditorController::save()
{
    if (m_appController == nullptr || !m_appController->isLibraryOpen() || m_currentGameId <= 0) {
        emit saveFailed(QStringLiteral("No game is loaded."));
        return false;
    }

    Database *db = m_appController->database();
    const bool updated = db->updateGame(
        m_currentGameId,
        m_pendingFields.value(QStringLiteral("publisher")).toString(),
        m_pendingFields.value(QStringLiteral("developer")).toString(),
        m_pendingFields.value(QStringLiteral("releaseDate")).toString(),
        m_pendingFields.value(QStringLiteral("description")).toString(),
        m_pendingFields.value(QStringLiteral("genres")).toString(),
        m_pendingFields.value(QStringLiteral("players")).toString(),
        m_pendingFields.value(QStringLiteral("rating")).toFloat());

    if (!updated) {
        emit saveFailed(QStringLiteral("Failed to save the current metadata record."));
        return false;
    }

    m_pendingFields.clear();
    setDirty(false);
    m_currentGame = buildGameMap(m_currentGameId);
    emit currentGameChanged();
    emit gameSaved();
    emit libraryChanged();
    return true;
}

void MetadataEditorController::discard()
{
    load(m_currentGameId);
}

QVariantMap MetadataEditorController::buildGameMap(int gameId) const
{
    QVariantMap result;
    if (m_appController == nullptr || !m_appController->isLibraryOpen() || gameId <= 0) {
        return result;
    }

    const QVariantMap match = m_appController->selectedMatch();
    result.insert(QStringLiteral("gameId"), gameId);
    result.insert(QStringLiteral("title"), match.value(QStringLiteral("title")));
    result.insert(QStringLiteral("region"), match.value(QStringLiteral("region")));
    result.insert(QStringLiteral("publisher"), match.value(QStringLiteral("publisher")));
    result.insert(QStringLiteral("developer"), match.value(QStringLiteral("developer")));
    result.insert(QStringLiteral("releaseDate"), match.value(QStringLiteral("releaseYear")).toInt() > 0
        ? QString::number(match.value(QStringLiteral("releaseYear")).toInt())
        : QString());
    result.insert(QStringLiteral("description"), match.value(QStringLiteral("description")));
    result.insert(QStringLiteral("genres"), match.value(QStringLiteral("genre")));
    result.insert(QStringLiteral("players"), match.value(QStringLiteral("players")));
    result.insert(QStringLiteral("rating"), match.value(QStringLiteral("rating")));
    return result;
}

void MetadataEditorController::setDirty(bool dirty)
{
    if (m_dirty == dirty) {
        return;
    }

    m_dirty = dirty;
    emit dirtyChanged();
}

} // namespace Remus