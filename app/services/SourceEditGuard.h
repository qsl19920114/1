#pragma once
#include "domain/Snapshot.h"
namespace qvw::services {
// Protects only a single authored file. It never restores over a different
// external edit; the Studio revision check remains the write authority.
class SourceEditGuard {
public:
    bool arm(const QString &root,const domain::Snapshot &snapshot,const QString &relativePath,
             const QString &newText,QString *error);
    bool restore(QString *error);
    void clear();
private:
    QString m_root,m_relative;
    QByteArray m_before,m_after;
    bool m_armed=false;
};
}
