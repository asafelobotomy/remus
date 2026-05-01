#include "verification_engine.h"

#include <QSqlDatabase>

namespace Remus {

VerificationEngine::VerificationEngine(Database *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
    // Schema creation requires an open connection.  Defer silently if the
    // library has not been opened yet; callers must connect libraryOpened to
    // createVerificationSchema() so that the schema is applied on first open.
    if (m_database && m_database->database().isOpen()) {
        createVerificationSchema();
    }
}

VerificationEngine::~VerificationEngine()
{
    if (m_compendiumConnectionName.isEmpty()) {
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(m_compendiumConnectionName, false);
        if (db.isValid() && db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(m_compendiumConnectionName);
}

} // namespace Remus
