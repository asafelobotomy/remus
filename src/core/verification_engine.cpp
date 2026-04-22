#include "verification_engine.h"

#include <QSqlDatabase>

namespace Remus {

VerificationEngine::VerificationEngine(Database *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
    createVerificationSchema();
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
