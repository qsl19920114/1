#pragma once
#include <QProcessEnvironment>
#include <QStringList>

namespace qvw::infra {
struct HypitInvocation {
    QString program;
    QStringList arguments;
    QString error;
    bool ok() const { return error.isEmpty(); }
};
class RuntimePaths {
public:
    static QString resourceRoot(const QString &executableDirectory = {});
    static QString templateDirectory(const QString &executableDirectory = {});
    static QProcessEnvironment processEnvironment(const QStringList &explicitToolPaths = {});
    static QString resolveTool(const QString &name, const QProcessEnvironment &environment);
    static HypitInvocation hypitInvocation(const QString &launcher, const QString &nodePath,
                                           bool nodeExplicit, const QStringList &arguments);
};
}
