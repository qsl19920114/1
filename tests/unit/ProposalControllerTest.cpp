#include "controllers/ProposalController.h"
#include "backend/hypit/SnapshotMapper.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>
#include <functional>
#include <memory>
using namespace qvw;
class ProposalHttp : public QObject {
public:
    QTcpServer server;int revision=1,writes=0,gets=0,status=200;QString value="old";bool stall=false,failGet=false;
    ProposalHttp() {
        connect(&server,&QTcpServer::newConnection,this,[this] {
            while(auto *socket=server.nextPendingConnection()) {
                auto data=std::make_shared<QByteArray>();auto handled=std::make_shared<bool>(false);
                connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
                connect(socket,&QTcpSocket::readyRead,this,[this,socket,data,handled] {
                    *data+=socket->readAll();const auto end=data->indexOf("\r\n\r\n");if(*handled||end<0)return;
                    int size=0;for(const auto &line:data->left(end).split('\n'))if(line.toLower().startsWith("content-length:"))size=line.mid(15).trimmed().toInt();
                    if(data->size()<end+4+size)return;*handled=true;
                    if(data->startsWith("GET ")) {++gets;if(stall)return;respond(socket,failGet?500:200,failGet?QByteArray("{\"error\":\"bad\"}"):payload());return;}
                    ++writes;
                    if(status==409){++revision;value="external";respond(socket,409,"{\"error\":\"conflict\"}");return;}
                    if(status==422){revision+=2;respond(socket,422,"{\"error\":\"compile rejected\"}");return;}
                    const auto json=QJsonDocument::fromJson(data->mid(end+4)).object();value=json["value"].toString();++revision;
                    respond(socket,200,QJsonDocument(QJsonObject{{"revision",revision}}).toJson());
                });
            }
        });
    }
    static void respond(QTcpSocket *socket,int code,const QByteArray &body) {
        socket->write("HTTP/1.1 "+QByteArray::number(code)+" Result\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);socket->disconnectFromHost();
    }
    QByteArray payload() const {
        QJsonObject field{{"id","parameter:title"},{"binding","title"},{"control","text"},{"value",value},{"edit",QJsonObject{}}};
        QJsonObject clip{{"id","cover"},{"inspector",QJsonArray{field}}};
        QJsonObject source{{"path","scene.svml"},{"text",value},{"files",QJsonArray{QJsonObject{{"path","scene.svml"},{"text",value}}}}};
        return QJsonDocument(QJsonObject{{"revision",revision},{"source",source},{"space",QJsonObject{}},{"tracks",QJsonArray{QJsonObject{{"id","track"},{"clips",QJsonArray{clip}}}}}}).toJson();
    }
    domain::Snapshot snapshot() const {return backend::hypit::mapSessionPayload(payload()).snapshot;}
    bool attach(controllers::EditorController &editor,const QString &root) {
        if(!server.listen(QHostAddress::LocalHost,0))return false;
        editor.attach(QUrl(QString("http://127.0.0.1:%1").arg(server.serverPort())),root);editor.acceptSnapshot(snapshot());return true;
    }
};
class CallbackProvider : public services::IProposalProvider {
public:
    std::function<void()> callback;
    bool generate(const QString &,const domain::Snapshot &s,const domain::Project &,domain::EditProposal *p,QString *) const override {
        p->revision=s.revision;p->sourceFingerprint=s.sourceFingerprint;p->entityId="cover";p->fieldId="parameter:title";
        p->value="injected";p->origin=domain::ProposalOrigin::ExternalUnverified;if(callback)callback();return true;
    }
};
class ProposalControllerTest : public QObject {
    Q_OBJECT
    domain::Project project(const QString &root) {domain::Project p;p.rootPath=root;p.templateId="title-card";p.sourcePath="scene.svml";return p;}
    QByteArray json(const domain::Snapshot &s,const QString &value="imported") {
        domain::EditProposal p;p.revision=s.revision;p.sourceFingerprint=s.sourceFingerprint;p.entityId="cover";p.fieldId="parameter:title";p.value=value;
        return p.toJson();
    }
private slots:
    void confirmationUsesEditorHistoryAndNeverWritesEarly() {
        QTemporaryDir dir;ProposalHttp http;controllers::EditorController editor;QVERIFY(http.attach(editor,dir.path()));
        controllers::ProposalController proposals(editor);proposals.setProject(project(dir.path()));
        QSignalSpy ok(&editor,&controllers::EditorController::operationSucceeded),fail(&proposals,&controllers::ProposalController::failed);
        proposals.generateDemo("标题改为new");QVERIFY(proposals.pending());QVERIFY(proposals.summary().contains("old"));QVERIFY(proposals.summary().contains("new"));QVERIFY(proposals.summary().contains("模拟"));
        QCOMPARE(http.writes,0);QCOMPARE(http.gets,0);QCOMPARE(editor.snapshot().sourceFiles.value("scene.svml"),QString("old"));
        proposals.confirm();QVERIFY(!proposals.pending());QTRY_COMPARE(ok.size(),1);QCOMPARE(http.writes,1);QCOMPARE(http.value,QString("new"));
        proposals.confirm();QCOMPARE(fail.size(),1);QCOMPARE(http.writes,1);editor.undo();QTRY_COMPARE(ok.size(),2);QCOMPARE(http.value,QString("old"));
    }
    void importedJsonAlwaysHasUnverifiedOrigin() {
        QTemporaryDir dir;ProposalHttp http;controllers::EditorController editor;QVERIFY(http.attach(editor,dir.path()));controllers::ProposalController p(editor);p.setProject(project(dir.path()));
        auto j=QJsonDocument::fromJson(json(editor.snapshot())).object();j["origin"]="real-model";p.importJson(QJsonDocument(j).toJson());
        QVERIFY(p.pending());QVERIFY(p.summary().contains("未核验"));QVERIFY(!p.summary().contains("real-model"));QCOMPARE(http.writes,0);
        p.discard();QVERIFY(!p.pending());QCOMPARE(http.writes,0);
    }
    void snapshotChangeProjectSwitchAndCloseInvalidate() {
        QTemporaryDir dir;ProposalHttp http;controllers::EditorController editor;QVERIFY(http.attach(editor,dir.path()));controllers::ProposalController p(editor);p.setProject(project(dir.path()));QSignalSpy fail(&p,&controllers::ProposalController::failed);
        p.generateDemo("标题改为new");QVERIFY(p.pending());auto changed=http.snapshot();changed.revision++;editor.acceptSnapshot(changed);QVERIFY(!p.pending());p.confirm();QCOMPARE(http.writes,0);
        p.generateDemo("标题改为new");QVERIFY(p.pending());p.setProject(project(dir.path()));QVERIFY(!p.pending());
        p.generateDemo("标题改为new");QVERIFY(p.pending());editor.clear();QVERIFY(!p.pending());p.confirm();QCOMPARE(http.writes,0);
        p.generateDemo("标题改为new");QVERIFY(!p.pending());QVERIFY(fail.size()>=3);
    }
    void malformedStaleReadonlyAndMissingProjectCannotPropose() {
        QTemporaryDir dir;ProposalHttp http;controllers::EditorController editor;QVERIFY(http.attach(editor,dir.path()));controllers::ProposalController p(editor);QSignalSpy fail(&p,&controllers::ProposalController::failed);
        p.generateDemo("标题改为new");QVERIFY(!p.pending());p.setProject(project(dir.path()));p.importJson("{");QVERIFY(!p.pending());
        auto stale=editor.snapshot();stale.revision--;p.importJson(json(stale));QVERIFY(!p.pending());
        auto readonly=editor.snapshot();readonly.tracks[0].clips[0].inspector[0].writable=false;editor.acceptSnapshot(readonly);p.importJson(json(readonly));QVERIFY(!p.pending());
        QCOMPARE(fail.size(),4);QCOMPARE(http.writes,0);
    }
    void anotherProjectCannotReuseEditorsVersionAndFingerprint() {
        QTemporaryDir dir,other;ProposalHttp http;controllers::EditorController editor;QVERIFY(http.attach(editor,dir.path()));
        controllers::ProposalController p(editor);p.setProject(project(other.path()));QSignalSpy fail(&p,&controllers::ProposalController::failed);
        p.generateDemo("标题改为new");QVERIFY(!p.pending());p.importJson(json(editor.snapshot()));QVERIFY(!p.pending());
        QCOMPARE(fail.size(),2);QCOMPARE(http.gets,0);QCOMPARE(http.writes,0);
    }
    void busyCreationAndConfirmationCannotWrite() {
        QTemporaryDir dir;ProposalHttp http;controllers::EditorController editor;QVERIFY(http.attach(editor,dir.path()));controllers::ProposalController p(editor);p.setProject(project(dir.path()));QSignalSpy fail(&p,&controllers::ProposalController::failed);
        p.generateDemo("标题改为new");QVERIFY(p.pending());http.stall=true;editor.refresh();QVERIFY(editor.isBusy());p.confirm();QCOMPARE(http.writes,0);
        p.generateDemo("标题改为other");QVERIFY(!p.pending());p.importJson(json(editor.snapshot()));QVERIFY(!p.pending());QCOMPARE(fail.size(),3);editor.clear();
    }
    void unreadyStateInvalidatesEvenWhenOldSnapshotIsRetained() {
        QTemporaryDir dir;ProposalHttp http;controllers::EditorController editor;QVERIFY(http.attach(editor,dir.path()));controllers::ProposalController p(editor);p.setProject(project(dir.path()));
        p.generateDemo("标题改为new");QVERIFY(p.pending());http.failGet=true;editor.refresh();QTRY_VERIFY(!editor.isBusy());QVERIFY(editor.snapshot().isLoaded());QVERIFY(!p.pending());
        p.generateDemo("标题改为new");QVERIFY(!p.pending());QCOMPARE(http.writes,0);
    }
    void imageRemovedBeforeConfirmationIsRejected() {
        QTemporaryDir dir;ProposalHttp http;controllers::EditorController editor;QVERIFY(http.attach(editor,dir.path()));
        auto s=editor.snapshot();auto &field=s.tracks[0].clips[0].inspector[0];field.binding="image";field.id="parameter:image";editor.acceptSnapshot(s);
        auto projectValue=project(dir.path());QDir(dir.path()).mkpath("assets");QFile f(dir.filePath("assets/one.png"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("image");f.close();domain::Asset asset;asset.path="assets/one.png";asset.mime="image/png";projectValue.assets.append(asset);
        controllers::ProposalController p(editor);p.setProject(projectValue);QSignalSpy fail(&p,&controllers::ProposalController::failed);p.generateDemo("图片使用第1张");QVERIFY(p.pending());
        QVERIFY(QFile::remove(f.fileName()));p.confirm();QVERIFY(!p.pending());QCOMPARE(fail.size(),1);QCOMPARE(http.writes,0);
    }
    void reentrantClearOnConsumedSignalPreventsConfirmation() {
        QTemporaryDir dir;ProposalHttp http;controllers::EditorController editor;QVERIFY(http.attach(editor,dir.path()));controllers::ProposalController p(editor);p.setProject(project(dir.path()));
        p.generateDemo("标题改为new");QVERIFY(p.pending());connect(&p,&controllers::ProposalController::proposalChanged,&p,[&](const QString &,bool pending){if(!pending)p.clearProject();});
        p.confirm();QTest::qWait(30);QCOMPARE(http.gets,0);QCOMPARE(http.writes,0);QVERIFY(!p.pending());
    }
    void reentrantSnapshotOnConsumedSignalPreventsConfirmation() {
        QTemporaryDir dir;ProposalHttp http;controllers::EditorController editor;QVERIFY(http.attach(editor,dir.path()));controllers::ProposalController p(editor);p.setProject(project(dir.path()));
        p.generateDemo("标题改为new");QVERIFY(p.pending());connect(&p,&controllers::ProposalController::proposalChanged,&p,[&](const QString &,bool pending){if(!pending){auto s=http.snapshot();s.revision++;editor.acceptSnapshot(s);}});
        p.confirm();QTest::qWait(30);QCOMPARE(http.gets,0);QCOMPARE(http.writes,0);
    }
    void providerCannotPublishAfterReentrantCloseOrChooseDemoOrigin() {
        QTemporaryDir dir;ProposalHttp http;controllers::EditorController editor;QVERIFY(http.attach(editor,dir.path()));controllers::ProposalController p(editor);p.setProject(project(dir.path()));
        auto provider=std::make_shared<CallbackProvider>();p.setProvider(provider);p.generateDemo("test");QVERIFY(p.pending());QVERIFY(p.summary().contains("模拟"));
        provider->callback=[&]{p.clearProject();};p.generateDemo("test");QVERIFY(!p.pending());QCOMPARE(http.writes,0);
    }
    void editorConflictAndRejectionRemainEditorFailures_data() {QTest::addColumn<int>("status");QTest::newRow("409")<<409;QTest::newRow("422")<<422;}
    void editorConflictAndRejectionRemainEditorFailures() {
        QFETCH(int,status);QTemporaryDir dir;ProposalHttp http;http.status=status;controllers::EditorController editor;QVERIFY(http.attach(editor,dir.path()));controllers::ProposalController p(editor);p.setProject(project(dir.path()));
        QSignalSpy fail(&editor,&controllers::EditorController::failed),ok(&editor,&controllers::EditorController::operationSucceeded);p.generateDemo("标题改为new");QVERIFY(p.pending());p.confirm();
        QTRY_COMPARE(fail.size(),1);QCOMPARE(ok.size(),0);QCOMPARE(http.writes,1);QVERIFY(!p.pending());
    }
};
QTEST_GUILESS_MAIN(ProposalControllerTest)
#include "ProposalControllerTest.moc"
