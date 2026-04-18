#include "verification_engine.h"

namespace Remus {

VerificationEngine::VerificationEngine(Database *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
    createVerificationSchema();
}

} // namespace Remus
