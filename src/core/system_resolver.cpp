#include "system_resolver.h"
#include "constants/providers.h"

namespace Remus {

using namespace Constants;
using namespace Constants::Providers;
using namespace Constants::Systems;

QString SystemResolver::displayName(int systemId) {
    const Constants::Systems::SystemDef *system = Constants::Systems::getSystem(systemId);
    return system ? system->displayName : QStringLiteral("Unknown");
}

QString SystemResolver::internalName(int systemId) {
    const Constants::Systems::SystemDef *system = Constants::Systems::getSystem(systemId);
    return system ? system->internalName : QStringLiteral("Unknown");
}

QString SystemResolver::providerName(int systemId, const QString &providerId) {
    static QMap<int, QMap<QString, QString>> mappings = providerMappings();

    // Check if we have a mapping for this system
    if (!mappings.contains(systemId)) {
        return QString();
    }

    // Check if we have a mapping for this provider
    const QMap<QString, QString> &providerMap = mappings[systemId];
    if (!providerMap.contains(providerId)) {
        // Fallback: for providers like Hasheous, use internal name
        return internalName(systemId);
    }

    return providerMap[providerId];
}

int SystemResolver::systemIdByName(const QString &internalName) {
    return Constants::Systems::getSystemIdByName(internalName);
}

QString SystemResolver::resolveSystemName(const QString &name) {
    // Try direct internal name lookup first
    int id = systemIdByName(name);
    if (id != 0) {
        return internalName(id);
    }

    // Try DAT name lookup
    id = systemIdByDatName(name);
    if (id != 0) {
        return internalName(id);
    }

    return name;
}

bool SystemResolver::isValidSystem(int systemId) {
    return Constants::Systems::getSystem(systemId) != nullptr;
}

} // namespace Remus
