#include "external_tool_runner.h"

namespace Remus {

ExternalToolRunner::ExternalToolRunner(QObject *parent)
    : QObject(parent)
{
}

void ExternalToolRunner::cancel()
{
    m_cancelled = true;
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->terminate();
        m_process->waitForFinished(3000);
        if (m_process->state() == QProcess::Running) {
            m_process->kill();
        }
    }
}

bool ExternalToolRunner::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

ExternalToolRunner::ProcessResult ExternalToolRunner::runProcess(const QString &program,
                                                                 const QStringList &args,
                                                                 int timeoutMs)
{
    ProcessResult result;
    QProcess process;

    process.start(program, args);
    result.started = process.waitForStarted(timeoutMs);
    if (!result.started) {
        result.exitCode = -1;
        return result;
    }

    result.finished = process.waitForFinished(timeoutMs);
    result.exitCode = process.exitCode();
    result.exitStatus = process.exitStatus();
    result.stdOutput = QString::fromUtf8(process.readAllStandardOutput());
    result.stdError = QString::fromUtf8(process.readAllStandardError());
    return result;
}

ExternalToolRunner::ProcessResult ExternalToolRunner::runProcessTracked(const QString &program,
                                                                        const QStringList &args,
                                                                        int timeoutMs)
{
    ProcessResult result;
    QProcess process;
    m_process = &process;

    process.start(program, args);
    result.started = process.waitForStarted(10000);
    if (!result.started) {
        m_process = nullptr;
        result.exitCode = -1;
        return result;
    }

    result.finished = process.waitForFinished(timeoutMs);
    result.exitCode = process.exitCode();
    result.exitStatus = process.exitStatus();
    result.stdOutput = QString::fromUtf8(process.readAllStandardOutput());
    result.stdError = QString::fromUtf8(process.readAllStandardError());
    m_process = nullptr;
    return result;
}

} // namespace Remus
