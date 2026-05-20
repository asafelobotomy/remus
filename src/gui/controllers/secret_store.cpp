#include "secret_store.h"

#include <QEventLoop>
#include <qt6keychain/keychain.h>

namespace Remus {

static const QString kService = QStringLiteral("remus");

QString SecretStore::read(const QString &key)
{
    QKeychain::ReadPasswordJob job(kService);
    job.setAutoDelete(false);
    job.setInsecureFallback(false);
    job.setKey(key);

    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();

    if (job.error() != QKeychain::NoError)
        return {};
    return job.textData();
}

bool SecretStore::write(const QString &key, const QString &value)
{
    QKeychain::WritePasswordJob job(kService);
    job.setAutoDelete(false);
    job.setInsecureFallback(false);
    job.setKey(key);
    job.setTextData(value);

    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();

    return job.error() == QKeychain::NoError;
}

void SecretStore::remove(const QString &key)
{
    QKeychain::DeletePasswordJob job(kService);
    job.setAutoDelete(false);
    job.setInsecureFallback(false);
    job.setKey(key);

    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();
}

} // namespace Remus
