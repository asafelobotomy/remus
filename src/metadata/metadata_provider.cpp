#include "metadata_provider.h"

namespace Remus {

MetadataProvider::MetadataProvider(QObject *parent)
    : QObject(parent)
{
}

void MetadataProvider::setCredentials(const QString &username, const QString &password)
{
    m_username = username;
    m_password = password;
    m_authenticated = !username.isEmpty();
}

bool MetadataProvider::isAvailable()
{
    // Default implementation - assume available
    // Subclasses can override to ping API
    return true;
}

GameMetadata MetadataProvider::getBySerial(const QString & /*serial*/, const QString & /*system*/)
{
    // Default: provider does not support serial-based lookup.
    return {};
}

} // namespace Remus
