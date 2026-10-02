#pragma once
#include "domain/Snapshot.h"
#include <QObject>
#include <QStringList>
namespace qvw::services {
class MediaValidation:public QObject {
    Q_OBJECT
public:
    explicit MediaValidation(QObject *parent=nullptr);
    ~MediaValidation() override;
    bool start(const QString &path,const domain::CanvasSpace &expected,const QString &ffprobe,const QString &ffmpeg);
    void cancel();
signals:
    void validated();
    void failed(const QString &message);
    void commandFinished(const QString &program,const QStringList &arguments,int code);
private:
    class State;State *m;
};
}
