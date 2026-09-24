#include "StudioProcess.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#endif

namespace qvw::backend::hypit {

StudioProcess::StudioProcess(QObject *parent) : QObject(parent)
{
    m_startupTimer.setSingleShot(true);
    m_startupTimer.setInterval(30000);
#ifdef Q_OS_UNIX
    // Own a fresh session/group, so descendants such as Vite's esbuild service
    // can be cleaned up without touching another Studio or shared worker.
    // Only async-signal-safe calls are allowed between fork and exec.
    m_process.setChildProcessModifier([] {
        if (::setsid() == -1)
            ::_exit(127);
    });
#endif
    connect(&m_process, &QProcess::started, this, [this] {
#ifdef Q_OS_UNIX
        m_ownedGroup = m_process.processId();
#endif
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        consume(m_stdout, m_process.readAllStandardOutput(), true);
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        consume(m_stderr, m_process.readAllStandardError(), false);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (!m_stopping)
            failOnce(tr("Studio process error: %1").arg(m_process.errorString()));
        if (error == QProcess::FailedToStart)
            finishOnce(-1, QProcess::NormalExit);
    });
    connect(&m_process, &QProcess::finished, this, &StudioProcess::finishOnce);
    connect(&m_startupTimer, &QTimer::timeout, this, [this] {
        failOnce(tr("Studio did not report a local URL within 30 seconds. Check the process output and project configuration."));
        stop();
    });
}

StudioProcess::~StudioProcess()
{
    stop();
}

bool StudioProcess::start(const QString &launcher, const QString &workspace,
                          const QString &run, const QString &runtime, int requestedPort)
{
    if (m_active || m_process.state() != QProcess::NotRunning) {
        emit failed(tr("Studio is already starting or running."));
        return false;
    }
    const QFileInfo programInfo(launcher);
    const QFileInfo workspaceInfo(workspace);
    const QFileInfo runInfo(run);
    const QFileInfo runtimeInfo(runtime);
    QString error;
    if (launcher.trimmed().isEmpty() || !programInfo.isFile() || !programInfo.isExecutable())
        error = tr("Studio launcher is not an executable file: %1").arg(launcher);
    else if (workspace.trimmed().isEmpty() || !workspaceInfo.isDir() || !workspaceInfo.isReadable())
        error = tr("Studio workspace is not a readable directory: %1").arg(workspace);
    else if (run.trimmed().isEmpty() || !runInfo.isFile() || !runInfo.isReadable())
        error = tr("Studio Run is not a readable file: %1").arg(run);
    else if (runtime.trimmed().isEmpty() || !runtimeInfo.isFile() || !runtimeInfo.isReadable())
        error = tr("Studio runtime profile is not a readable file: %1").arg(runtime);
    else if (requestedPort < 1 || requestedPort > 65535)
        error = tr("Studio port must be between 1 and 65535.");
    if (!error.isEmpty()) {
        emit failed(error);
        return false;
    }

    m_active = true;
    m_ready = false;
    m_failed = false;
    m_stopping = false;
    m_ownedGroup = 0;
    m_stdout.clear();
    m_stderr.clear();
    m_process.setProgram(programInfo.absoluteFilePath());
    m_process.setArguments({QStringLiteral("studio"), QStringLiteral("--run"), runInfo.absoluteFilePath(),
                            QStringLiteral("--workspace"), workspaceInfo.absoluteFilePath(),
                            QStringLiteral("--runtime"), runtimeInfo.absoluteFilePath(),
                            QStringLiteral("--port"), QString::number(requestedPort)});
    m_process.setWorkingDirectory(workspaceInfo.absoluteFilePath());
    // Upstream start.ts:61 uses INIT_CWD in preference to cwd; every supplied
    // path is absolute, so an inherited package-manager cwd cannot rebase it.
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_startupTimer.start();
    m_process.start();
    return true;
}

void StudioProcess::consume(QByteArray &buffer, const QByteArray &bytes, bool stdoutChannel, bool flush)
{
    buffer.append(bytes);
    while (!buffer.isEmpty()) {
        const auto newline = buffer.indexOf('\n');
        if (newline < 0 && !flush) {
            // Bound an uncooperative launcher's unterminated log line.
            if (buffer.size() > 1024 * 1024) {
                emit output(QString::fromUtf8(buffer));
                buffer.clear();
            }
            break;
        }
        const auto count = newline < 0 ? buffer.size() : newline + 1;
        QString line = QString::fromUtf8(buffer.left(count));
        buffer.remove(0, count);
        // Vite adds terminal colour; strip CSI and OSC sequences only after a
        // complete byte line has arrived (UTF-8 and escapes can be fragmented).
        static const QRegularExpression escapes(QStringLiteral("\x1b(?:\\[[0-?]*[ -/]*[@-~]|\\][^\\x07]*(?:\\x07|\x1b\\\\))"));
        line.remove(escapes);
        line = line.trimmed();
        emit output(line);
        if (!stdoutChannel || m_ready || m_failed || m_stopping)
            continue;
        // Contract §1 / start.ts:172: consume Vite's Local line only. Never
        // accept Network, Comments or arbitrary URLs printed by a dependency.
        static const QRegularExpression local(QStringLiteral(
            "^(?:[➜→]\\s*)?Local:\\s+(http://(?:localhost|127\\.0\\.0\\.1|\\[::1\\]):([0-9]{1,5})/)\\s*$"));
        const auto match = local.match(line);
        if (!match.hasMatch())
            continue;
        const int port = match.captured(2).toInt();
        const QUrl url(match.captured(1), QUrl::StrictMode);
        if (!url.isValid() || port < 1 || port > 65535)
            continue;
        m_ready = true;
        m_startupTimer.stop();
        emit ready(url);
    }
}

void StudioProcess::failOnce(const QString &message)
{
    if (!m_active || m_failed)
        return;
    m_failed = true;
    m_startupTimer.stop();
    emit failed(message);
}

void StudioProcess::finishOnce(int exitCode, QProcess::ExitStatus status)
{
    if (!m_active)
        return;
    m_startupTimer.stop();
    consume(m_stdout, m_process.readAllStandardOutput(), true, true);
    consume(m_stderr, m_process.readAllStandardError(), false, true);
    if (!m_stopping && !m_failed) {
        if (status == QProcess::CrashExit)
            failOnce(tr("Studio crashed (exit code %1).").arg(exitCode));
        else if (!m_ready)
            failOnce(tr("Studio exited before reporting a local URL (exit code %1).").arg(exitCode));
        else if (exitCode != 0)
            failOnce(tr("Studio exited with code %1.").arg(exitCode));
    }
    cleanOwnedGroup();
    const QString program = m_process.program();
    const QStringList args = m_process.arguments();
    m_active = false;
    emit output(tr("Studio process exited with code %1.").arg(exitCode));
    emit commandFinished(program, args, exitCode);
    emit finished(exitCode);
}

void StudioProcess::cleanOwnedGroup()
{
#ifdef Q_OS_UNIX
    if (m_ownedGroup > 0) {
        // This group was created for this invocation before exec. No process
        // search, executable-name matching or external worker termination.
        ::kill(-pid_t(m_ownedGroup), SIGTERM);
        ::kill(-pid_t(m_ownedGroup), SIGKILL);
    }
#endif
    m_ownedGroup = 0;
}

void StudioProcess::stop()
{
    m_startupTimer.stop();
    m_stopping = true;
    if (m_process.state() == QProcess::Starting)
        m_process.waitForStarted(1000);
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        if (!m_process.waitForFinished(1500)) {
            cleanOwnedGroup();
            m_process.kill();
            m_process.waitForFinished(1000);
        }
    }
    cleanOwnedGroup();
}

bool StudioProcess::isRunning() const
{
    return m_process.state() != QProcess::NotRunning;
}

qint64 StudioProcess::processId() const
{
    return m_process.processId();
}
}
