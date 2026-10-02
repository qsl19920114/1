#include "controllers/ExportController.h"
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>
using namespace qvw;
namespace {
bool writeFile(const QString &path,const QByteArray &bytes){QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();}
QByteArray readFile(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
QString executable(const QString &path,const QByteArray &bytes){if(!writeFile(path,bytes))return {};QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);return path;}
struct Fixture {
    QTemporaryDir temp;domain::Project project;domain::Snapshot snapshot;infra::AppConfig config;QString probe,decode;
    Fixture(){
        const auto root=temp.filePath("project");QDir().mkpath(root);project.rootPath=QFileInfo(root).canonicalFilePath();project.sourcePath="main.svml";project.runPath="main.svrun";project.runtimePath="hypit.runtime.json";
        writeFile(root+"/main.svml","source");writeFile(root+"/main.svrun","run");writeFile(root+"/hypit.runtime.json",R"({"format":"hypit.runtime-local@1","dataRoot":".hypit/local","endpoints":{"media":{"use":"@hypit/provider-media-local","config":{}}}})");
        snapshot.revision=9;snapshot.sourceFiles={{"main.svml","source"}};snapshot.sourceFingerprint=QCryptographicHash::hash(QJsonDocument(QJsonObject{{"main.svml","source"}}).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);snapshot.space={640,360,60,2,30};
        config.distributionPath=temp.filePath("distribution");QDir().mkpath(config.distributionPath);config.launcherPath=executable(config.distributionPath+"/hypit",R"PY(#!/usr/bin/python3
import json,sys,pathlib,time
base=pathlib.Path(__file__).parent
mode=(base/'mode').read_text() if (base/'mode').exists() else 'success'
cmd=sys.argv[1]
with (base/'calls').open('a') as f: f.write(json.dumps(sys.argv[1:])+'\n')
bid='bld_fixture_1'
def dto(state='working',result='missing',outcome=None):
    b={'id':bid,'work':{'state':state},'result':{'state':result}}
    if outcome: b['work']['outcome']=outcome
    return b
if cmd=='plan':
    if mode=='stallplan': time.sleep(2)
    if mode=='clierror': print(json.dumps({'format':'hypit.cli-error@1','ok':False,'error':{'code':'SYNTAX_ERROR','message':'activation syntax error: Unexpected token'}}));sys.exit(1)
    if mode=='malformed': print('not json');sys.exit(0)
    p={'format':'hypit.cli-plan@1','ok':True,'providerRequestCount':1 if mode=='paid' else 0,'localRequestCount':1,'requestIssueCount':0,'unresolvedRequestCount':0,'unsupportedRequestCount':0,'providers':[{'status':'resolved','use':'@hypit/provider-media-local','pricing':{'kind':'local'}}],'preflight':{'ok':True}}
    if mode=='missing_preflight': p.pop('preflight')
    if mode=='missing_providers': p.pop('providers')
    if mode=='diagnostic': p['ok']=False;p['preflight']={'ok':False,'diagnostics':[{'message':'encoder unavailable'}]}
    print(json.dumps(p))
elif cmd=='build':
    if mode=='lateack': (base/'accepted').write_text('yes');time.sleep(2)
    b=dto()
    if mode=='badid': b['id']='../escape'
    if mode=='buildfailed': b=dto('done','failed','failed')
    print(json.dumps({'format':'hypit.cli-build@1','build':b}));sys.exit(1 if mode=='buildfailed' else 0)
elif cmd=='status':
    counter=base/'counter'; n=int(counter.read_text())+1 if counter.exists() else 1;counter.write_text(str(n))
    if mode=='stall': time.sleep(2)
    b=dto('done','complete','complete') if n>1 else dto()
    if mode in ('working','slowcancel'): b=dto()
    if mode=='failed': b=dto('done','failed','failed')
    if mode=='missingoutput': b=dto('done','unavailable','complete')
    if mode=='finishing': b=dto('done','open','complete') if n==1 else dto('done','complete','complete')
    if mode=='wrongid': b['id']='bld_other'
    if mode=='badshape': b.pop('result')
    if (base/'cancelled').exists(): b=dto('done','cancelled','cancelled') if n>1 else dto()
    print(json.dumps({'format':'hypit.cli-status@1','build':b}));sys.exit(1 if mode=='failed' else 0)
elif cmd=='cancel':
    (base/'cancelled').write_text('yes');(base/'counter').write_text('0')
    if mode=='slowcancel': time.sleep(1.5)
    print(json.dumps({'format':'hypit.cli-cancel@1','requested':mode!='cancelunrequested','build':dto()}))
elif cmd=='activity':
    print(json.dumps({'format':'hypit.cli-activity@1','builds':[{'id':bid,'work':{'state':'working'},'phases':{}}]}))
elif cmd=='builds':
    print(json.dumps({'format':'hypit.cli-builds@1','builds':[{'id':bid,'outcome':'complete','run':'main.svrun'}]}))
elif cmd=='get':
    path=sys.argv[sys.argv.index('--to')+1]
    if mode!='nooutput': pathlib.Path(path).write_bytes(b'validated movie')
    print(json.dumps({'format':'hypit.cli-get@1','build':'bld_other' if mode=='staleget' else bid,'output':'final.video','path':path,'kind':'resource'}))
)PY");
        probe=executable(config.distributionPath+"/probe",R"PY(#!/usr/bin/python3
print('{"streams":[{"codec_type":"video","codec_name":"h264","width":640,"height":360,"avg_frame_rate":"30/1"}],"format":{"format_name":"mov,mp4,m4a,3gp,3g2,mj2","duration":"2"}}')
)PY");
        decode=executable(config.distributionPath+"/decode",R"PY(#!/usr/bin/python3
import pathlib,sys
sys.exit(1 if (pathlib.Path(__file__).parent/'decodefail').exists() else 0)
)PY");
    }
    void mode(const QByteArray &value){writeFile(config.distributionPath+"/mode",value);}
    QString destination()const{return temp.filePath("result.mp4");}
    int calls(const QByteArray &command)const{int n=0;for(const auto &line:readFile(config.distributionPath+"/calls").split('\n')){const auto a=QJsonDocument::fromJson(line).array();if(!a.isEmpty()&&a[0].toString().toUtf8()==command)++n;}return n;}
    void attach(controllers::ExportController &c){c.setMediaTools(probe,decode);c.setProject(project);}
};
}
class ExportControllerTest:public QObject {
    Q_OBJECT
private slots:
    void successRequiresAllLayers() {
        Fixture f;infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy completed(&c,&controllers::ExportController::completed),failed(&c,&controllers::ExportController::failed);
        c.startExport(f.snapshot,f.destination());QTRY_COMPARE_WITH_TIMEOUT(completed.size(),1,5000);QCOMPARE(failed.size(),0);QCOMPARE(c.task().phase,QString("complete"));QCOMPARE(c.task().revision,9);QVERIFY(!c.isBusy());QCOMPARE(readFile(f.destination()),QByteArray("validated movie"));QCOMPARE(f.calls("build"),1);
        for(const auto &line:readFile(f.config.distributionPath+"/calls").split('\n')) {const auto a=QJsonDocument::fromJson(line).array();if(a.isEmpty())continue;QStringList args;for(const auto &v:a)args<<v.toString();QVERIFY(args.contains("--workspace"));QVERIFY(args.contains("--json"));if(args[0]!="get")QVERIFY(args.contains("--runtime"));QVERIFY(!args.contains("--follow"));}
    }
    void rejectsPlan_data(){QTest::addColumn<QByteArray>("mode");QTest::newRow("paid")<<QByteArray("paid");QTest::newRow("malformed")<<QByteArray("malformed");QTest::newRow("preflight")<<QByteArray("missing_preflight");QTest::newRow("providers")<<QByteArray("missing_providers");}
    void rejectsPlan(){QFETCH(QByteArray,mode);Fixture f;f.mode(mode);infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy fail(&c,&controllers::ExportController::failed),ok(&c,&controllers::ExportController::completed);c.startExport(f.snapshot,f.destination());QTRY_COMPARE_WITH_TIMEOUT(fail.size(),1,1500);QCOMPARE(ok.size(),0);QCOMPARE(f.calls("build"),0);}
    void failedOrMalformedBuild_data(){QTest::addColumn<QByteArray>("mode");QTest::newRow("badid")<<QByteArray("badid");QTest::newRow("buildfailure")<<QByteArray("buildfailed");QTest::newRow("terminalfailure")<<QByteArray("failed");QTest::newRow("wrongid")<<QByteArray("wrongid");QTest::newRow("shape")<<QByteArray("badshape");QTest::newRow("missingresult")<<QByteArray("missingoutput");}
    void failedOrMalformedBuild(){QFETCH(QByteArray,mode);Fixture f;f.mode(mode);infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy fail(&c,&controllers::ExportController::failed),ok(&c,&controllers::ExportController::completed);c.startExport(f.snapshot,f.destination());QTRY_COMPARE_WITH_TIMEOUT(fail.size(),1,2500);QCOMPARE(ok.size(),0);QCOMPARE(f.calls("get"),0);QVERIFY(!QFileInfo::exists(f.destination()));}
    void resourceAndDecodeFailure_data(){QTest::addColumn<QByteArray>("mode");QTest::newRow("nooutput")<<QByteArray("nooutput");QTest::newRow("staleid")<<QByteArray("staleget");QTest::newRow("decode")<<QByteArray("decode");}
    void resourceAndDecodeFailure(){QFETCH(QByteArray,mode);Fixture f;f.mode(mode);if(mode=="decode")writeFile(f.config.distributionPath+"/decodefail","yes");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy fail(&c,&controllers::ExportController::failed),ok(&c,&controllers::ExportController::completed);c.startExport(f.snapshot,f.destination());QTRY_COMPARE_WITH_TIMEOUT(fail.size(),1,5000);QCOMPARE(ok.size(),0);QVERIFY(!QFileInfo::exists(f.destination()));}
    void cancelWaitsForTerminal() {
        Fixture f;f.mode("working");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy ok(&c,&controllers::ExportController::completed),fail(&c,&controllers::ExportController::failed);
        c.startExport(f.snapshot,f.destination());QTRY_VERIFY_WITH_TIMEOUT(!c.task().buildId.isEmpty(),1500);c.cancelBuild();QTRY_COMPARE_WITH_TIMEOUT(f.calls("cancel"),1,1000);QVERIFY(c.isBusy());QVERIFY(c.task().phase!="cancelled");QTRY_COMPARE_WITH_TIMEOUT(c.task().phase,QString("cancelled"),4000);QCOMPARE(ok.size(),0);QCOMPARE(fail.size(),0);QCOMPARE(f.calls("get"),0);QVERIFY(!c.isBusy());
    }
    void resumeUsesSameBuildAndFrozenSource() {
        Fixture f;f.mode("working");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy ok(&c,&controllers::ExportController::completed),fail(&c,&controllers::ExportController::failed);
        c.startExport(f.snapshot,f.destination());QTRY_VERIFY_WITH_TIMEOUT(!c.task().buildId.isEmpty(),1500);const auto id=c.task().buildId;const auto workspace=c.task().workspace;c.stopObserving();QVERIFY(!c.isBusy());c.clearProject();writeFile(f.project.rootPath+"/main.svml","edited");f.mode("success");c.setProject(f.project);QCOMPARE(c.task().phase,QString("stopped"));QVERIFY(c.task().active);c.resume();QTRY_COMPARE_WITH_TIMEOUT(ok.size(),1,5000);QCOMPARE(fail.size(),0);QCOMPARE(c.task().buildId,id);QCOMPARE(f.calls("build"),1);QCOMPARE(readFile(workspace+"/main.svml"),QByteArray("source"));
    }
    void completedWorkWaitsForFinishedResult() {
        Fixture f;f.mode("finishing");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy ok(&c,&controllers::ExportController::completed),fail(&c,&controllers::ExportController::failed);
        c.startExport(f.snapshot,f.destination());QTRY_COMPARE_WITH_TIMEOUT(f.calls("status"),1,2000);QTest::qWait(100);QCOMPARE(f.calls("get"),0);QCOMPARE(fail.size(),0);QVERIFY(c.isBusy());QTRY_COMPARE_WITH_TIMEOUT(ok.size(),1,4000);QCOMPARE(fail.size(),0);
    }
    void reentrantCloseCannotSubmit() {
        Fixture f;infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);connect(&c,&controllers::ExportController::busyChanged,&c,[&](bool busy){if(busy)c.clearProject();});c.startExport(f.snapshot,f.destination());QTest::qWait(80);QCOMPARE(f.calls("plan"),0);QCOMPARE(f.calls("build"),0);QVERIFY(!c.isBusy());
    }
    void failedTaskPersistenceCannotReportCompletion() {
        Fixture f;infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy fail(&c,&controllers::ExportController::failed),ok(&c,&controllers::ExportController::completed);
        connect(&c,&controllers::ExportController::taskChanged,&c,[&](const domain::ExportTask &task){if(task.phase!="validating")return;QFile::remove(f.project.rootPath+"/.workbench/export-task.json");QFile::link(f.project.rootPath+"/main.svml",f.project.rootPath+"/.workbench/export-task.json");});
        c.startExport(f.snapshot,f.destination());QTRY_COMPARE_WITH_TIMEOUT(fail.size(),1,5000);QCOMPARE(ok.size(),0);QVERIFY(!c.isBusy());QCOMPARE(readFile(f.project.rootPath+"/main.svml"),QByteArray("source"));
    }
    void closeBeforeBuildAcknowledgementRecoversWithoutResubmit() {
        Fixture f;f.mode("lateack");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy fail(&c,&controllers::ExportController::failed),ok(&c,&controllers::ExportController::completed);
        c.startExport(f.snapshot,f.destination());QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(f.config.distributionPath+"/accepted"),1500);const auto workspace=c.task().workspace;QVERIFY(c.task().buildId.isEmpty());c.stopObserving();c.clearProject();c.setProject(f.project);QVERIFY(c.task().active);QCOMPARE(c.task().workspace,workspace);QCOMPARE(c.task().phase,QString("stopped"));
        c.startExport(f.snapshot,f.destination());QCOMPARE(fail.size(),1);QCOMPARE(f.calls("build"),1);c.resume();QTRY_COMPARE_WITH_TIMEOUT(ok.size(),1,5000);QCOMPARE(c.task().buildId,QString("bld_fixture_1"));QCOMPARE(f.calls("build"),1);QCOMPARE(f.calls("activity"),1);
    }
    void stopPlanningRecordsThatNoBuildWasSubmitted() {
        Fixture f;f.mode("stallplan");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy messages(&c,&controllers::ExportController::message),ok(&c,&controllers::ExportController::completed);
        c.startExport(f.snapshot,f.destination());QTRY_COMPARE_WITH_TIMEOUT(f.calls("plan"),1,1500);c.stopObserving();QString combined;for(const auto &row:messages)combined+=row[0].toString();QVERIFY(combined.contains("尚未提交"));c.clearProject();c.setProject(f.project);QVERIFY(!c.task().active);QVERIFY(c.task().buildId.isEmpty());QCOMPARE(f.calls("build"),0);f.mode("success");c.startExport(f.snapshot,f.destination());QTRY_COMPARE_WITH_TIMEOUT(ok.size(),1,5000);QCOMPARE(f.calls("build"),1);
    }
    void diagnosticsAreVisible() {
        Fixture f;f.mode("diagnostic");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy fail(&c,&controllers::ExportController::failed),messages(&c,&controllers::ExportController::message);c.startExport(f.snapshot,f.destination());QTRY_COMPARE_WITH_TIMEOUT(fail.size(),1,1500);QString combined;for(const auto &row:messages)combined+=row[0].toString();QVERIFY(combined.contains("encoder unavailable"));QCOMPARE(f.calls("build"),0);
    }
    void pinnedCliErrorMessageIsVisible() {
        Fixture f;f.mode("clierror");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy fail(&c,&controllers::ExportController::failed),messages(&c,&controllers::ExportController::message);c.startExport(f.snapshot,f.destination());QTRY_COMPARE_WITH_TIMEOUT(fail.size(),1,1500);QVERIFY(fail[0][0].toString().contains("activation syntax error"));QString combined;for(const auto &row:messages)combined+=row[0].toString();QVERIFY(combined.contains("activation syntax error"));QCOMPARE(f.calls("build"),0);
    }
    void reentrantCancellationDoesNotStartParallelStatus() {
        Fixture f;f.mode("slowcancel");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy fail(&c,&controllers::ExportController::failed);bool requested=false;
        connect(&c,&controllers::ExportController::taskChanged,&c,[&](const domain::ExportTask &task){if(task.phase=="working"&&!requested){requested=true;c.cancelBuild();}});c.startExport(f.snapshot,f.destination());QTRY_COMPARE_WITH_TIMEOUT(c.task().phase,QString("cancelled"),6000);QCOMPARE(fail.size(),0);QCOMPARE(f.calls("cancel"),1);
    }
    void unknownIdRecoveryReentrantCancellationKeepsCancelCommand() {
        Fixture f;f.mode("lateack");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy fail(&c,&controllers::ExportController::failed),ok(&c,&controllers::ExportController::completed);
        c.startExport(f.snapshot,f.destination());QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(f.config.distributionPath+"/accepted"),1500);c.stopObserving();c.clearProject();c.setProject(f.project);QVERIFY(c.task().active);QVERIFY(c.task().buildId.isEmpty());f.mode("slowcancel");bool requested=false;
        connect(&c,&controllers::ExportController::taskChanged,&c,[&](const domain::ExportTask &task){if(task.phase=="working"&&!task.buildId.isEmpty()&&!requested){requested=true;c.cancelBuild();}});
        c.resume();QTRY_VERIFY_WITH_TIMEOUT(requested,1500);QTest::qWait(100);QCOMPARE(fail.size(),0);QTRY_COMPARE_WITH_TIMEOUT(c.task().phase,QString("cancelled"),6000);QCOMPARE(f.calls("build"),1);QCOMPARE(f.calls("activity"),1);QCOMPARE(f.calls("cancel"),1);QCOMPARE(ok.size(),0);
    }
    void unconfirmedCancelMessageCannotPublishIntoAnotherProject() {
        Fixture f,other;f.mode("cancelunrequested");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy ok(&c,&controllers::ExportController::completed);bool switched=false;
        connect(&c,&controllers::ExportController::message,&c,[&](const QString &message){if(!message.contains("未确认取消请求"))return;switched=true;c.clearProject();c.setProject(other.project);});
        c.startExport(f.snapshot,f.destination());QTRY_VERIFY_WITH_TIMEOUT(!c.task().buildId.isEmpty(),1500);c.cancelBuild();QTRY_VERIFY_WITH_TIMEOUT(switched,1500);QCOMPARE(c.task().phase,QString("idle"));QVERIFY(c.task().buildId.isEmpty());QVERIFY(!c.task().active);QVERIFY(!c.isBusy());QTest::qWait(80);QCOMPARE(ok.size(),0);QVERIFY(!QFileInfo::exists(other.project.rootPath+"/.workbench/export-task.json"));
    }
    void stageDoesNotOverwriteCopiedUserFileAndStillRecovers() {
        Fixture f;QVERIFY(writeFile(f.project.rootPath+"/export-stage.mp4","user export"));infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy ok(&c,&controllers::ExportController::completed),fail(&c,&controllers::ExportController::failed);
        c.startExport(f.snapshot,f.destination());QTRY_COMPARE_WITH_TIMEOUT(ok.size(),1,5000);const auto workspace=c.task().workspace;const auto id=c.task().buildId;QCOMPARE(readFile(workspace+"/export-stage.mp4"),QByteArray("user export"));QCOMPARE(readFile(f.project.rootPath+"/export-stage.mp4"),QByteArray("user export"));
        c.clearProject();c.setProject(f.project);QCOMPARE(c.task().buildId,id);c.resume();QTRY_COMPARE_WITH_TIMEOUT(ok.size(),2,5000);QCOMPARE(fail.size(),0);QCOMPARE(f.calls("build"),1);QCOMPARE(readFile(workspace+"/export-stage.mp4"),QByteArray("user export"));
    }
    void sourceAndUnsafeDestinationRejected() {
        Fixture f;infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);f.attach(c);QSignalSpy fail(&c,&controllers::ExportController::failed);c.startExport(f.snapshot,f.project.rootPath+"/main.svml");QCOMPARE(fail.size(),1);QCOMPARE(readFile(f.project.rootPath+"/main.svml"),QByteArray("source"));writeFile(f.project.rootPath+"/main.svml","external");c.startExport(f.snapshot,f.destination());QCOMPARE(fail.size(),2);QCOMPARE(f.calls("build"),0);
    }
};
QTEST_GUILESS_MAIN(ExportControllerTest)
#include "ExportControllerTest.moc"
