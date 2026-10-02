#pragma once
#include "domain/Project.h"
#include <QObject>
#include <QProcessEnvironment>
#include <QStringList>
Q_DECLARE_METATYPE(qvw::domain::Asset)
namespace qvw::services {
// Validates a private immutable copy, the first 240 frame timestamps, and full video-stream decode.
// Video-story samples these eight seconds at 30 fps; audio is not imported.
// Caller project remains unchanged until imported().
class VideoAssetImporter:public QObject {
    Q_OBJECT
public:
    explicit VideoAssetImporter(QObject *parent=nullptr);
    ~VideoAssetImporter() override;
    bool start(const domain::Project &project,const QString &source,const QString &ffprobe,const QString &ffmpeg);
    void setEnvironment(const QProcessEnvironment &environment);
    void cancel();
    bool active() const;
signals:
    void imported(const qvw::domain::Project &project,const qvw::domain::Asset &asset);
    void failed(const QString &message);
    void stateChanged(bool active);
    void commandFinished(const QString &program,const QStringList &arguments,int code);
private:
    class State;State *m;
};
}
