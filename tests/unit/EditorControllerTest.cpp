#include "controllers/EditorController.h"
#include "backend/hypit/SnapshotMapper.h"
#include <QTest>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QFile>
#include <memory>
using qvw::controllers::EditorController;
class EditorHttp : public QObject {
public:
    QTcpServer server;
    int revision=1, writes=0, gets=0, writeStatus=200;
    QString value="old", source="old", root, control="text";
    QList<QJsonValue> wireValues;
    bool changeClosure=false,changeSharedDependency=false;
    bool stallGet=false,stallWrite=false,failGet=false,compileFail=false,externalAfterSource=false,failAfterWrite=false,stallAfterWrite=false;
    int ackOffset=0;
    QList<QPointer<QTcpSocket>> waiting;
    EditorHttp() {
        connect(&server,&QTcpServer::newConnection,this,[this] {
            while(auto *s=server.nextPendingConnection()) {
                auto data=std::make_shared<QByteArray>();auto done=std::make_shared<bool>(false);
                connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);
                connect(s,&QTcpSocket::readyRead,this,[this,s,data,done] {
                    *data+=s->readAll();auto p=data->indexOf("\r\n\r\n");if(*done||p<0)return;
                    int n=0;for(auto line:data->left(p).split('\n'))if(line.toLower().startsWith("content-length:"))n=line.mid(15).trimmed().toInt();
                    if(data->size()<p+4+n)return;*done=true;
                    if(data->startsWith("GET ")) {
                        ++gets;if(stallGet){waiting.append(s);return;}
                        if(!root.isEmpty()){QFile f(root+"/scene.svml");if(f.open(QIODevice::ReadOnly))source=QString::fromUtf8(f.readAll());}
                        if(failGet||(compileFail&&source=="broken")){respond(s,500,QJsonDocument(QJsonObject{{"error","compile error"},{"revision",revision}}).toJson());return;}
                        respond(s,200,payload());return;
                    }
                    ++writes;auto j=QJsonDocument::fromJson(data->mid(p+4)).object();
                    if(writeStatus==409){++revision;source="external";respond(s,409,R"({"error":"conflict"})");return;}
                    if(writeStatus==422){revision+=2;respond(s,422,R"({"error":"rejected"})");return;}
                    ++revision;
                    if(data->startsWith("PUT ")) {
                        source=j["text"].toString();if(!root.isEmpty()){QFile f(root+"/scene.svml");f.open(QIODevice::WriteOnly);f.write(source.toUtf8());}
                        if(externalAfterSource){QFile f(root+"/scene.svml");f.open(QIODevice::WriteOnly);f.write("external");failGet=true;}
                    } else {wireValues.append(j["value"]);value=j["value"].toVariant().toString();source=value;}
                    if(failAfterWrite)failGet=true;if(stallAfterWrite)stallGet=true;
                    if(stallWrite){waiting.append(s);return;}
                    respond(s,writeStatus==500?500:(data->startsWith("PUT ")?202:200),QJsonDocument(QJsonObject{{"revision",revision+ackOffset}}).toJson());
                });
            }
        });
    }
    bool listen(){return server.listen(QHostAddress::LocalHost,0);}
    QUrl url() const{return QUrl(QString("http://127.0.0.1:%1").arg(server.serverPort()));}
    static void respond(QTcpSocket *s,int status,const QByteArray &body){s->write("HTTP/1.1 "+QByteArray::number(status)+" Result\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);s->disconnectFromHost();}
    QByteArray payload() const {
        QJsonObject field{{"id","title"},{"binding","title"},{"control",control},{"value",value},{"edit",QJsonObject{}}};
        QJsonObject clip{{"id","clip"},{"inspector",QJsonArray{field}}};
        QJsonArray files{QJsonObject{{"path","scene.svml"},{"text",source}}};
        if(changeClosure) {
            files.append(QJsonObject{{"path",source=="old"?"old-dependency.svs":"new-dependency.svs"},{"text","dependency"}});
            files.append(QJsonObject{{"path","shared.svs"},{"text",changeSharedDependency&&source!="old"?"externally changed":"stable"}});
        }
        return QJsonDocument(QJsonObject{{"revision",revision},{"source",QJsonObject{{"path","scene.svml"},{"text",source},{"files",files}}},{"space",QJsonObject{}},{"tracks",QJsonArray{QJsonObject{{"id","track"},{"clips",QJsonArray{clip}}}}}}).toJson();
    }
    qvw::domain::Snapshot snapshot() const{return qvw::backend::hypit::mapSessionPayload(payload()).snapshot;}
    void attach(EditorController &editor){editor.attach(url(),root);editor.acceptSnapshot(snapshot());}
};
class EditorControllerTest : public QObject {
    Q_OBJECT
private slots:
    void editUndoRedo() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;h.attach(e);QSignalSpy ok(&e,&EditorController::operationSucceeded),state(&e,&EditorController::stateChanged);
        e.edit("clip","title","new");QTRY_COMPARE(ok.size(),1);QCOMPARE(h.writes,1);QCOMPARE(e.snapshot().revision,2);QVERIFY(state.last()[2].toBool());
        e.undo();QTRY_COMPARE(ok.size(),2);QCOMPARE(h.value,QString("old"));QVERIFY(state.last()[3].toBool());
        e.redo();QTRY_COMPARE(ok.size(),3);QCOMPARE(h.value,QString("new"));
    }
    void typedWireValuesFromLiteralSnapshots_data() {
        QTest::addColumn<QString>("control");QTest::addColumn<QString>("literal");QTest::addColumn<QVariant>("next");QTest::addColumn<QVariant>("previous");
        QTest::newRow("number")<<QString("number")<<QString("18")<<QVariant(24.0)<<QVariant(18.0);
        QTest::newRow("boolean")<<QString("boolean")<<QString("true")<<QVariant(false)<<QVariant(true);
    }
    void typedWireValuesFromLiteralSnapshots() {
        QFETCH(QString,control);QFETCH(QString,literal);QFETCH(QVariant,next);QFETCH(QVariant,previous);
        EditorHttp h;h.control=control;h.value=literal;QVERIFY(h.listen());EditorController e;h.attach(e);QSignalSpy ok(&e,&EditorController::operationSucceeded),fail(&e,&EditorController::failed);
        e.edit("clip","title",next);QTRY_COMPARE_WITH_TIMEOUT(ok.size(),1,1000);QCOMPARE(fail.size(),0);QCOMPARE(h.wireValues.last(),QJsonValue::fromVariant(next));
        e.undo();QTRY_COMPARE_WITH_TIMEOUT(ok.size(),2,1000);QCOMPARE(h.wireValues.last(),QJsonValue::fromVariant(previous));QCOMPARE(h.value,literal);
        e.redo();QTRY_COMPARE_WITH_TIMEOUT(ok.size(),3,1000);QCOMPARE(h.wireValues.last(),QJsonValue::fromVariant(next));
    }
    void sourceDependencyClosureMayChange() {
        QTemporaryDir dir;QFile f(dir.filePath("scene.svml"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("old");f.close();
        EditorHttp h;h.root=dir.path();h.changeClosure=true;QVERIFY(h.listen());EditorController e;h.attach(e);QSignalSpy ok(&e,&EditorController::operationSucceeded),fail(&e,&EditorController::failed);
        e.replaceSource("scene.svml","valid");QTRY_COMPARE_WITH_TIMEOUT(ok.size(),1,1000);QCOMPARE(fail.size(),0);
        QVERIFY(e.snapshot().sourceFiles.contains("new-dependency.svs"));QVERIFY(!e.snapshot().sourceFiles.contains("old-dependency.svs"));
        e.undo();QTRY_COMPARE_WITH_TIMEOUT(ok.size(),2,1000);QVERIFY(e.snapshot().sourceFiles.contains("old-dependency.svs"));
    }
    void sourceStillRejectsChangedRetainedDependency() {
        QTemporaryDir dir;QFile f(dir.filePath("scene.svml"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("old");f.close();
        EditorHttp h;h.root=dir.path();h.changeClosure=true;h.changeSharedDependency=true;QVERIFY(h.listen());EditorController e;h.attach(e);QSignalSpy ok(&e,&EditorController::operationSucceeded),fail(&e,&EditorController::failed);
        e.replaceSource("scene.svml","valid");QTRY_COMPARE_WITH_TIMEOUT(fail.size(),1,1000);QCOMPARE(ok.size(),0);
    }
    void preflightFingerprintConflictWithoutRevisionChange() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;h.attach(e);QSignalSpy fail(&e,&EditorController::failed);h.source="external";
        e.edit("clip","title","new");QTRY_COMPARE(fail.size(),1);QCOMPARE(h.writes,0);QCOMPARE(e.snapshot().sourceFiles["scene.svml"],QString("external"));
    }
    void conflictClearsHistory() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;h.attach(e);QSignalSpy ok(&e,&EditorController::operationSucceeded),fail(&e,&EditorController::failed),state(&e,&EditorController::stateChanged);
        e.edit("clip","title","new");QTRY_COMPARE(ok.size(),1);h.writeStatus=409;e.undo();QTRY_COMPARE(fail.size(),1);QVERIFY(!state.last()[2].toBool());QVERIFY(!state.last()[3].toBool());QCOMPARE(ok.size(),1);
    }
    void rejectedRollbackKeepsHistoryAndNewRevision() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;h.attach(e);QSignalSpy ok(&e,&EditorController::operationSucceeded),fail(&e,&EditorController::failed),state(&e,&EditorController::stateChanged);
        e.edit("clip","title","new");QTRY_COMPARE(ok.size(),1);h.writeStatus=422;e.undo();QTRY_COMPARE(fail.size(),1);QVERIFY(state.last()[2].toBool());QCOMPARE(e.snapshot().revision,4);
        h.writeStatus=200;e.undo();QTRY_COMPARE(ok.size(),2);QCOMPARE(h.value,QString("old"));
    }
    void timeoutNeverClaimsSuccessEvenIfServerApplied() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;e.setTimeoutMs(60);h.attach(e);h.stallWrite=true;QSignalSpy fail(&e,&EditorController::failed),ok(&e,&EditorController::operationSucceeded),state(&e,&EditorController::stateChanged);
        e.edit("clip","title","new");QTRY_COMPARE(fail.size(),1);QCOMPARE(h.writes,1);QCOMPARE(ok.size(),0);QCOMPARE(e.snapshot().sourceFiles["scene.svml"],QString("new"));QVERIFY(!state.last()[2].toBool());
    }
    void cancellationAndParallelRequest() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;h.attach(e);h.stallGet=true;QSignalSpy fail(&e,&EditorController::failed),ok(&e,&EditorController::operationSucceeded),snap(&e,&EditorController::snapshotReady);
        e.edit("clip","title","new");QTRY_COMPARE(h.gets,1);e.edit("clip","title","other");QCOMPARE(fail.size(),1);QVERIFY(e.isBusy());
        e.clear();for(auto s:h.waiting)if(s)EditorHttp::respond(s,200,h.payload());QTest::qWait(80);QCOMPARE(h.writes,0);QCOMPARE(ok.size(),0);QCOMPARE(snap.size(),0);QVERIFY(!e.snapshot().isLoaded());
    }
    void readFailureRequiresExplicitRefresh() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;h.attach(e);h.failGet=true;QSignalSpy fail(&e,&EditorController::failed),state(&e,&EditorController::stateChanged);
        e.edit("clip","title","new");QTRY_COMPARE(fail.size(),1);QVERIFY(!state.last()[0].toBool());e.edit("clip","title","new");QCOMPARE(fail.size(),2);QCOMPARE(h.writes,0);
        h.failGet=false;e.refresh();QTRY_VERIFY(state.last()[0].toBool());
    }
    void reentrantCloseOnStateCannotPublishOrWrite() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;h.attach(e);QSignalSpy snap(&e,&EditorController::snapshotReady);
        connect(&e,&EditorController::stateChanged,&e,[&](bool,bool busy,bool,bool){if(busy)e.clear();});e.edit("clip","title","new");QTest::qWait(50);QCOMPARE(h.gets,0);QCOMPARE(h.writes,0);QCOMPARE(snap.size(),0);
    }
    void sourceSuccessAndUndo() {
        QTemporaryDir dir;QFile f(dir.filePath("scene.svml"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("old");f.close();
        EditorHttp h;h.root=dir.path();QVERIFY(h.listen());EditorController e;h.attach(e);QSignalSpy ok(&e,&EditorController::operationSucceeded);
        e.replaceSource("scene.svml","valid");QTRY_COMPARE(ok.size(),1);QCOMPARE(h.source,QString("valid"));e.undo();QTRY_COMPARE(ok.size(),2);QCOMPARE(h.source,QString("old"));
    }
    void sourceCompileFailureRestoresAndKeepsHistory() {
        QTemporaryDir dir;QFile f(dir.filePath("scene.svml"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("old");f.close();
        EditorHttp h;h.root=dir.path();h.compileFail=true;QVERIFY(h.listen());EditorController e;h.attach(e);QSignalSpy ok(&e,&EditorController::operationSucceeded),fail(&e,&EditorController::failed);
        e.replaceSource("scene.svml","broken");QTRY_COMPARE_WITH_TIMEOUT(fail.size(),1,2000);QCOMPARE(ok.size(),0);QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("old"));QCOMPARE(e.snapshot().sourceFiles["scene.svml"],QString("old"));QVERIFY(!e.isBusy());
    }
    void confirmationFailureDoesNotClaimSuccess() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;h.attach(e);h.failAfterWrite=true;QSignalSpy fail(&e,&EditorController::failed),ok(&e,&EditorController::operationSucceeded),state(&e,&EditorController::stateChanged);
        e.edit("clip","title","new");QTRY_COMPARE(fail.size(),1);QCOMPARE(h.writes,1);QCOMPARE(ok.size(),0);QVERIFY(!state.last()[0].toBool());
    }
    void wrongAcknowledgedRevisionClearsHistory() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;h.attach(e);h.ackOffset=1;QSignalSpy fail(&e,&EditorController::failed),ok(&e,&EditorController::operationSucceeded),state(&e,&EditorController::stateChanged);
        e.edit("clip","title","new");QTRY_COMPARE(fail.size(),1);QCOMPARE(ok.size(),0);QVERIFY(!state.last()[2].toBool());QCOMPARE(e.snapshot().revision,2);
    }
    void sourceUnknownOutcomeMustNotRestore() {
        QTemporaryDir dir;QFile f(dir.filePath("scene.svml"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("old");f.close();
        EditorHttp h;h.root=dir.path();h.stallWrite=true;QVERIFY(h.listen());EditorController e;e.setTimeoutMs(60);h.attach(e);QSignalSpy fail(&e,&EditorController::failed),ok(&e,&EditorController::operationSucceeded);
        e.replaceSource("scene.svml","valid");QTRY_COMPARE(fail.size(),1);QCOMPARE(ok.size(),0);QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("valid"));
    }
    void acceptedSourceWithLostConfirmationMustNotRestore() {
        QTemporaryDir dir;QFile f(dir.filePath("scene.svml"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("old");f.close();
        EditorHttp h;h.root=dir.path();h.stallAfterWrite=true;QVERIFY(h.listen());EditorController e;e.setTimeoutMs(60);h.attach(e);QSignalSpy fail(&e,&EditorController::failed),ok(&e,&EditorController::operationSucceeded);
        e.replaceSource("scene.svml","valid");QTRY_COMPARE_WITH_TIMEOUT(fail.size(),1,2000);QCOMPARE(ok.size(),0);QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("valid"));
    }
    void missingSourcesAndUnsupportedControlCannotWrite() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;h.attach(e);auto snapshot=h.snapshot();snapshot.sourceFiles.clear();snapshot.sourceFingerprint.clear();e.acceptSnapshot(snapshot);QSignalSpy fail(&e,&EditorController::failed);
        e.edit("clip","title","new");QCOMPARE(fail.size(),1);snapshot=h.snapshot();snapshot.tracks[0].clips[0].inspector[0].control=qvw::domain::ControlKind::Unsupported;e.acceptSnapshot(snapshot);
        e.edit("clip","title","new");QCOMPARE(fail.size(),2);QCOMPARE(h.gets,0);QCOMPARE(h.writes,0);
    }
    void readOnlyControlCannotWrite() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;h.attach(e);auto snapshot=h.snapshot();snapshot.tracks[0].clips[0].inspector[0].writable=false;e.acceptSnapshot(snapshot);QSignalSpy fail(&e,&EditorController::failed);
        e.edit("clip","title","new");QCOMPARE(fail.size(),1);QCOMPARE(h.writes,0);
    }
    void reentrantCloseOnCompletionStateDoesNotPublish() {
        EditorHttp h;QVERIFY(h.listen());EditorController e;h.attach(e);QSignalSpy snap(&e,&EditorController::snapshotReady),ok(&e,&EditorController::operationSucceeded);
        connect(&e,&EditorController::stateChanged,&e,[&](bool ready,bool busy,bool,bool){if(ready&&!busy)e.clear();});e.edit("clip","title","new");QTRY_COMPARE(h.writes,1);QTRY_VERIFY(!e.isBusy());QCOMPARE(snap.size(),0);QCOMPARE(ok.size(),0);
    }
    void sourceRecoveryDoesNotOverwriteExternalFile() {
        QTemporaryDir dir;QFile f(dir.filePath("scene.svml"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("old");f.close();
        EditorHttp h;h.root=dir.path();QVERIFY(h.listen());EditorController e;h.attach(e);h.externalAfterSource=true;QSignalSpy fail(&e,&EditorController::failed),ok(&e,&EditorController::operationSucceeded);
        connect(&e,&EditorController::message,&e,[](const QString&){});
        e.replaceSource("scene.svml","broken");QTRY_COMPARE(fail.size(),1);QCOMPARE(ok.size(),0);QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("external"));
    }
};
QTEST_GUILESS_MAIN(EditorControllerTest)
#include "EditorControllerTest.moc"
