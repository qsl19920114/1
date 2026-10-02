#pragma once
#include "domain/Project.h"
#include "services/VideoAssetImporter.h"
#include <QObject>
#include <QProcessEnvironment>
#include <optional>
Q_DECLARE_METATYPE(qvw::domain::Project)
namespace qvw::controllers {
class DocumentController : public QObject {
    Q_OBJECT
public:
    explicit DocumentController(QObject *parent=nullptr);
    bool create(const QString &templateDirectory,const QString &destination,const QString &name);
    bool open(const QString &manifest);
    bool save();
    bool importImage(const QString &path);
    void setMediaTools(const QString &ffprobe,const QString &ffmpeg,const QProcessEnvironment &environment);
    bool importVideo(const QString &path);
    bool importingVideo() const;
    void cancelVideoImport();
    void close();
    bool hasProject() const { return m_project.has_value(); }
    domain::Project project() const { return m_project.value_or(domain::Project{}); }
signals:
    void projectLoaded(const qvw::domain::Project &project);
    void projectChanged(const qvw::domain::Project &project);
    void documentClosed();
    void failed(const QString &message);
    void message(const QString &message);
    void videoImportStateChanged(bool importing);
    void assetImported(const qvw::domain::Asset &asset);
private:
    std::optional<domain::Project> m_project;
    services::VideoAssetImporter m_videoImporter;
    QString m_ffprobe,m_ffmpeg;
    quint64 m_documentGeneration=0,m_importGeneration=0;
};
}
