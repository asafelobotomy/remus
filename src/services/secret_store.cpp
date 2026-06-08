#include "secret_store.h"

#include <QEventLoop>
#include <QDebug>
#include <qt6keychain/keychain.h>

namespace Remus {

static const QString kService = QStringLiteral("remus");

SecretStore::ReadResult SecretStore::readWithStatus(const QString &key) {
    QKeychain::ReadPasswordJob job(kService);
    job.setAutoDelete(false);
    job.setInsecureFallback(false);
    job.setKey(key);

    QEventLoop loop;
    QObject::connect(&job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job.start();
    loop.exec();

    if (job.error() == QKeychain::NoError)
        return { ReadResult::Status::Found, job.textData(), { } };
    if (job.error() == QKeychain::EntryNotFound)
        return { ReadResult::Status::NotFound, { }, { } };
    return { ReadResult::Status::BackendError, { },
        QStringLiteral("Keychain error for '%1': %2").arg(key, job.errorString()) };
}

QString SecretStore::read(const QString &key) {
    const ReadResult r = readWithStatus(key);
    if (r.status == ReadResult::Status::BackendError)
        qWarning().noquote() << "SecretStore:" << r.errorMessage;
    return r.value;
}

bool SecretStore::write(const QString &key, const QString &value) {
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

void SecretStore::remove(const QString &key) {
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
