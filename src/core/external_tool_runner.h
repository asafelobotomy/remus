#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>

namespace Remus {

/**
 * @brief Base class for wrappers around external CLI tools (chdman, 7z, unzip, etc.)
 *
 * Provides shared process execution, cancellation, and status tracking.
 */
class ExternalToolRunner : public QObject {
    Q_OBJECT

public:
    explicit ExternalToolRunner(QObject *parent = nullptr);

    virtual void cancel();
    bool isRunning() const;

protected:
    struct ProcessResult {
        bool started = false;
        bool finished = false;
        int exitCode = -1;
        QProcess::ExitStatus exitStatus = QProcess::NormalExit;
        QString stdOutput;
        QString stdError;
    };

    virtual ProcessResult runProcess(const QString &program, const QStringList &args, int timeoutMs);
    virtual ProcessResult runProcessTracked(const QString &program, const QStringList &args, int timeoutMs);

    bool m_cancelled = false;
    QProcess *m_process = nullptr;
};

} // namespace Remus
