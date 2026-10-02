#include "JsonProcess.h"
#include <QJsonDocument>
#include <QPointer>
#include <QProcess>
#include <QTimer>

namespace qvw::infra {
class JsonProcess::State {
public:
    JsonProcess *owner;
    QPointer<QProcess> process;
    QTimer timer;
    QByteArray output, diagnostics;
    quint64 generation=0;
    explicit State(JsonProcess *o):owner(o) { timer.setSingleShot(true); }
    void dispose(QProcess *p) {
        timer.stop();process=nullptr;p->disconnect(owner);
        if(p->state()!=QProcess::NotRunning){p->kill();p->waitForFinished(500);}
        p->deleteLater();
    }
    void fail(QProcess *p,const QString &error) {
        if(process!=p)return;
        const auto generationBefore=generation;const auto program=p->program();const auto arguments=p->arguments();dispose(p);
        emit owner->commandFinished(program,arguments,-1);
        if(generation==generationBefore)emit owner->failed(error);
    }
    bool consume(QProcess *p) {
        p->setReadChannel(QProcess::StandardOutput);output+=p->read(4*1024*1024-output.size()+1);
        p->setReadChannel(QProcess::StandardError);diagnostics+=p->read(64*1024-diagnostics.size()+1);
        if(output.size()>4*1024*1024||diagnostics.size()>64*1024) {
            fail(p,QStringLiteral("命令输出超过限制（stdout 4 MiB / stderr 64 KiB）。"));return false;
        }
        return true;
    }
};
JsonProcess::JsonProcess(QObject *parent):QObject(parent),m(new State(this)) {
    connect(&m->timer,&QTimer::timeout,this,[this]{if(m->process)m->fail(m->process,QStringLiteral("命令超时，观察已停止。"));});
}
JsonProcess::~JsonProcess(){cancel();delete m;}
bool JsonProcess::start(const QString &program,const QStringList &arguments,const QString &cwd,int deadlineMs) {
    if(m->process||deadlineMs<=0)return false;
    ++m->generation;auto *p=new QProcess(this);m->process=p;m->output.clear();m->diagnostics.clear();
    p->setProgram(program);p->setArguments(arguments);p->setWorkingDirectory(cwd);p->setProcessChannelMode(QProcess::SeparateChannels);
    connect(p,&QProcess::readyReadStandardOutput,this,[this,p]{if(m->process==p)m->consume(p);});
    connect(p,&QProcess::readyReadStandardError,this,[this,p]{if(m->process==p)m->consume(p);});
    connect(p,&QProcess::errorOccurred,this,[this,p](QProcess::ProcessError error){
        if(m->process==p&&error==QProcess::FailedToStart)m->fail(p,QStringLiteral("命令无法启动：%1").arg(p->errorString()));
    });
    connect(p,&QProcess::finished,this,[this,p](int code,QProcess::ExitStatus status){
        if(m->process!=p||!m->consume(p))return;
        if(status==QProcess::CrashExit){m->fail(p,QStringLiteral("命令异常退出。"));return;}
        QJsonParseError error;const auto document=QJsonDocument::fromJson(m->output,&error);
        const auto program=p->program();const auto args=p->arguments();const auto diagnostics=QString::fromUtf8(m->diagnostics).trimmed();
        const auto generation=m->generation;m->dispose(p);emit commandFinished(program,args,code);if(generation!=m->generation)return;
        if(error.error!=QJsonParseError::NoError||!document.isObject()) {
            emit failed(QStringLiteral("命令没有返回严格的 JSON 对象：%1%2").arg(error.errorString(),diagnostics.isEmpty()?QString():QStringLiteral("；")+diagnostics));return;
        }
        emit result(document.object(),code);
    });
    m->timer.start(deadlineMs);p->start();return true;
}
void JsonProcess::cancel() {
    ++m->generation;
    if(!m->process)return;
    auto *p=m->process.data();const auto program=p->program();const auto arguments=p->arguments();m->dispose(p);
    emit commandFinished(program,arguments,-1);
}
bool JsonProcess::isRunning() const{return m->process;}
}
