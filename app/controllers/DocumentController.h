#pragma once
#include "domain/Project.h"
#include <QObject>
#include <optional>
Q_DECLARE_METATYPE(qvw::domain::Project)
namespace qvw::controllers {
class DocumentController : public QObject {
    Q_OBJECT
public:
    explicit DocumentController(QObject *parent = nullptr) : QObject(parent) {}
    bool create(const QString &templateDirectory,const QString &destination,const QString &name);
    bool open(const QString &manifest);
    bool save();
    bool importImage(const QString &path);
    void close();
    bool hasProject() const { return m_project.has_value(); }
    domain::Project project() const { return m_project.value_or(domain::Project{}); }
signals:
    void projectLoaded(const qvw::domain::Project &project);
    void projectChanged(const qvw::domain::Project &project);
    void documentClosed();
    void failed(const QString &message);
    void message(const QString &message);
private:
    std::optional<domain::Project> m_project;
};
}
