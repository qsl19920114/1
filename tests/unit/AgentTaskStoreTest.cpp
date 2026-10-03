#include "agent/AgentTaskStore.h"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif
class AgentTaskStoreTest : public QObject {
    Q_OBJECT
private slots:
    void specialFilesNeverBlockOrGetReplaced_data() {
        QTest::addColumn<QString>("operation");QTest::addColumn<QString>("filename");
        QTest::newRow("load-current")<<QString("load")<<QString("current.json");
        QTest::newRow("append-events")<<QString("append")<<QString("events.jsonl");
        QTest::newRow("save-current")<<QString("save")<<QString("current.json");
        QTest::newRow("special-lock")<<QString("save")<<QString("task.lock");
        QTest::newRow("special-rotation")<<QString("append")<<QString("events.previous.jsonl");
    }
    void specialFilesNeverBlockOrGetReplaced() {
#ifdef Q_OS_UNIX
        QFETCH(QString,operation);QFETCH(QString,filename);QTemporaryDir dir;
        QVERIFY(QDir().mkpath(dir.filePath(".workbench/agent")));
        const auto fifo=dir.filePath(".workbench/agent/"+filename);QCOMPARE(::mkfifo(QFile::encodeName(fifo).constData(),0600),0);
        if(filename=="events.previous.jsonl"){QFile events(dir.filePath(".workbench/agent/events.jsonl"));QVERIFY(events.open(QIODevice::WriteOnly));events.write(QByteArray(1024*1024,'x'));}
        QProcess probe;probe.start(QCoreApplication::applicationFilePath(),{"--special-probe",operation,dir.path()});
        const bool finished=probe.waitForFinished(2000);if(!finished){probe.kill();probe.waitForFinished(2000);}
        QVERIFY2(finished,"Task API blocked on a FIFO without a peer");QCOMPARE(probe.exitCode(),0);
        struct stat info{};QCOMPARE(::lstat(QFile::encodeName(fifo).constData(),&info),0);QVERIFY(S_ISFIFO(info.st_mode));
#else
        QSKIP("POSIX FIFO test");
#endif
    }
    void rejectsInternalSymlinksWithoutChangingSource() {
        for(const auto &name:QStringList{"current.json","events.jsonl","task.lock","events.previous.jsonl"}) {
            QTemporaryDir dir;qvw::domain::Project p;p.rootPath=dir.path();QString error;
            QVERIFY(QDir().mkpath(dir.filePath(".workbench/agent")));
            QFile source(dir.filePath("main.svml"));QVERIFY(source.open(QIODevice::WriteOnly));source.write("original source");source.close();
            QVERIFY(QFile::link(source.fileName(),dir.filePath(".workbench/agent/"+name)));
            if(name=="events.previous.jsonl"){QFile events(dir.filePath(".workbench/agent/events.jsonl"));QVERIFY(events.open(QIODevice::WriteOnly));events.write(QByteArray(1024*1024,'x'));}
            const bool result=name=="current.json"?qvw::agent::AgentTaskStore::save(p,{{"format","qvw.agent-task@1"}},&error):qvw::agent::AgentTaskStore::append(p,{{"event","write"}},&error);
            QVERIFY2(!result,qPrintable(name));
            QVERIFY(source.open(QIODevice::ReadOnly));QCOMPARE(source.readAll(),QByteArray("original source"));
        }
    }
    void rejectsDirectorySymlinkInsideProject() {
        QTemporaryDir dir;qvw::domain::Project p;p.rootPath=dir.path();QString error;
        QVERIFY(QDir().mkpath(dir.filePath("safe")));QVERIFY(QDir().mkpath(dir.filePath(".workbench")));
        QVERIFY(QFile::link(dir.filePath("safe"),dir.filePath(".workbench/agent")));
        QVERIFY(!qvw::agent::AgentTaskStore::save(p,{{"format","qvw.agent-task@1"}},&error));
        QVERIFY(QDir(dir.filePath("safe")).entryList(QDir::Files|QDir::Hidden|QDir::NoDotAndDotDot).isEmpty());
    }
    void roundTripAndAppendActualEvents() {
        QTemporaryDir dir;qvw::domain::Project p;p.rootPath=dir.path();QString error;
        QJsonObject state{{"format","qvw.agent-task@1"},{"taskId","task-1"},{"phase","paused"},{"completed",1}};
        QVERIFY2(qvw::agent::AgentTaskStore::save(p,state,&error),qPrintable(error));QJsonObject restored;
        QVERIFY(qvw::agent::AgentTaskStore::load(p,&restored,&error));QCOMPARE(restored,state);
        QVERIFY(qvw::agent::AgentTaskStore::append(p,{{"taskId","task-1"},{"tool","apply_approved_proposal"},{"revision",18}},&error));
        QFile events(dir.filePath(".workbench/agent/events.jsonl"));QVERIFY(events.open(QIODevice::ReadOnly));
        QVERIFY(QJsonDocument::fromJson(events.readLine()).isObject());
        state["goal"]=QString(256*1024,'x');QVERIFY(!qvw::agent::AgentTaskStore::save(p,state,&error));
    }
    void cannotFollowLinkedStateDirectory() {
        QTemporaryDir dir,external;qvw::domain::Project p;p.rootPath=dir.path();QVERIFY(QDir().mkpath(dir.filePath(".workbench")));
        QVERIFY(QFile::link(external.path(),dir.filePath(".workbench/agent")));QString error;QJsonObject state;
        QVERIFY(!qvw::agent::AgentTaskStore::save(p,{{"format","qvw.agent-task@1"}},&error));
        QVERIFY(!qvw::agent::AgentTaskStore::append(p,{{"event","write"}},&error));
        QVERIFY(QDir(external.path()).entryList(QDir::Files|QDir::Hidden|QDir::NoDotAndDotDot).isEmpty());
    }
};
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()==4&&args[1]=="--special-probe"){
        qvw::domain::Project p;p.rootPath=args[3];QString error;QJsonObject state;
        const bool accepted=args[2]=="load"?qvw::agent::AgentTaskStore::load(p,&state,&error):args[2]=="save"?qvw::agent::AgentTaskStore::save(p,{{"format","qvw.agent-task@1"}},&error):qvw::agent::AgentTaskStore::append(p,{{"event","write"}},&error);
        return !accepted&&error.contains(QStringLiteral("普通文件"))?0:2;
    }
    AgentTaskStoreTest test;return QTest::qExec(&test,argc,argv);
}
#include "AgentTaskStoreTest.moc"
