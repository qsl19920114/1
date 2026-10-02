#include "RuntimePaths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QFile>

namespace qvw::infra {
QString RuntimePaths::resourceRoot(const QString &executableDirectory) {
    const QDir executable(executableDirectory.isEmpty() ? QCoreApplication::applicationDirPath() : executableDirectory);
    if (executable.dirName() == QStringLiteral("MacOS") && executable.absolutePath().endsWith(QStringLiteral("/Contents/MacOS")))
        return QDir::cleanPath(executable.filePath(QStringLiteral("../Resources")));
    return QDir::cleanPath(executable.filePath(QStringLiteral("resources")));
}
QString RuntimePaths::templateDirectory(const QString &executableDirectory) {
    return QDir(resourceRoot(executableDirectory)).filePath(QStringLiteral("templates"));
}
QProcessEnvironment RuntimePaths::processEnvironment(const QStringList &explicitToolPaths) {
    auto environment = QProcessEnvironment::systemEnvironment();
    QStringList paths;
    for (const auto &tool : explicitToolPaths)
        if (!tool.isEmpty() && QDir::isAbsolutePath(tool)) paths << QFileInfo(tool).absolutePath();
    paths << QStringLiteral("/opt/homebrew/bin") << QStringLiteral("/usr/local/bin")
          << QStringLiteral("/usr/bin") << QStringLiteral("/bin") << QStringLiteral("/usr/sbin") << QStringLiteral("/sbin");
    for (const auto &path : environment.value(QStringLiteral("PATH")).split(QDir::listSeparator(), Qt::SkipEmptyParts))
        if (QDir::isAbsolutePath(path)) paths << path;
    paths.removeDuplicates();
    environment.insert(QStringLiteral("PATH"), paths.join(QDir::listSeparator()));
    return environment;
}
QString RuntimePaths::resolveTool(const QString &name, const QProcessEnvironment &environment) {
    return QStandardPaths::findExecutable(name, environment.value(QStringLiteral("PATH")).split(QDir::listSeparator(), Qt::SkipEmptyParts));
}
HypitInvocation RuntimePaths::hypitInvocation(const QString &launcher, const QString &nodePath,
                                              bool nodeExplicit, const QStringList &arguments) {
    HypitInvocation invocation{launcher, arguments, {}};
    const QFileInfo info(launcher);
    const auto suffix = info.suffix().toLower();
    QString script;
    if (suffix == QStringLiteral("mjs") || suffix == QStringLiteral("js")) script = launcher;
    else if (nodeExplicit) {
        // Fixed upstream 1af179d3f58284c2d6d3c1f63052172a4fe1b5a6,
        // hypit:1-11. Match the whole 404-byte official POSIX wrapper; a custom
        // shell wrapper may perform additional work and must never be bypassed.
        QFile wrapper(launcher);
        if (info.fileName() == QStringLiteral("hypit") && info.size() == 404
            && wrapper.open(QIODevice::ReadOnly)
            && QCryptographicHash::hash(wrapper.read(405), QCryptographicHash::Sha256).toHex()
                == QByteArrayLiteral("a76d1919a225d08c929cbe2a8d8183b62f1552b19c17a8caf782c905ac4ed0e4")) {
            script = QFileInfo(info.canonicalFilePath()).absoluteDir().filePath(QStringLiteral("bin/hypit.mjs"));
            if (!QFileInfo(script).isFile() || !QFileInfo(script).isReadable())
                invocation.error = QStringLiteral("官方 Hypit shell 启动器缺少可读取的 bin/hypit.mjs：%1").arg(script);
        } else {
            invocation.error = QStringLiteral("显式 tools.node 仅支持 .mjs/.js 或固定版本的官方 hypit shell 启动器；当前自定义可执行入口不能安全替换：%1。可省略 tools.node 使用其原启动方式。").arg(launcher);
        }
    }
    if (!invocation.ok() || script.isEmpty() || nodePath.isEmpty()) return invocation;
    invocation.program = nodePath;
    invocation.arguments.prepend(script);
    return invocation;
}
}
