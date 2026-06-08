#include "conversion_service.h"

namespace Remus {

bool ConversionService::isChdmanAvailable() const {
    return m_chdConverter->isChdmanAvailable();
}

QString ConversionService::getChdmanVersion() const {
    return m_chdConverter->getChdmanVersion();
}

void ConversionService::setChdmanPath(const QString &path) {
    m_chdConverter->setChdmanPath(path);
}

bool ConversionService::isDolphinToolAvailable() const {
    return m_rvzConverter->isDolphinToolAvailable();
}

QString ConversionService::getDolphinToolVersion() const {
    return m_rvzConverter->getDolphinToolVersion();
}

void ConversionService::setDolphinToolPath(const QString &path) {
    m_rvzConverter->setDolphinToolPath(path);
}

bool ConversionService::isMaxcsoAvailable() const {
    return m_csoConverter->isMaxcsoAvailable();
}

QString ConversionService::getMaxcsoVersion() const {
    return m_csoConverter->getMaxcsoVersion();
}

void ConversionService::setMaxcsoPath(const QString &path) {
    m_csoConverter->setMaxcsoPath(path);
}

QMap<ArchiveFormat, bool> ConversionService::getArchiveToolStatus() const {
    return m_archiveExtractor->getAvailableTools();
}

bool ConversionService::canExtract(const QString &path) const {
    return m_archiveExtractor->canExtract(path);
}

void ConversionService::cancel() {
    m_chdConverter->cancel();
    m_rvzConverter->cancel();
    m_csoConverter->cancel();
    m_archiveExtractor->cancel();
    m_archiveCreator->cancel();
}

bool ConversionService::isRunning() const {
    return m_chdConverter->isRunning() || m_rvzConverter->isRunning() || m_csoConverter->isRunning()
        || m_archiveExtractor->isRunning() || m_archiveCreator->isRunning();
}

} // namespace Remus