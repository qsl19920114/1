#include "controllers/ExportController.h"
#include "services/ExportWorkspace.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>
using namespace qvw;
namespace {
bool write(const QString &path,const QByteArray &data){QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(data)==data.size();}
QByteArray read(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
struct Fixture {
 QTemporaryDir temp;domain::Project project;domain::Snapshot snapshot;domain::ExportTask task;services::FrozenExport frozen;infra::AppConfig config;QString error;
 Fixture(){project.rootPath=temp.filePath("project");project.runPath="main.svrun";project.sourcePath="main.svml";project.runtimePath="hypit.runtime.json";write(project.rootPath+"/main.svml","source");write(project.rootPath+"/main.svrun","run");write(project.rootPath+"/assets/image.png","asset");write(project.rootPath+"/hypit.runtime.json",R"({"format":"hypit.runtime-local@1","dataRoot":".hypit/local","endpoints":{"media":{"use":"@hypit/provider-media-local","config":{}}}})");project.rootPath=QFileInfo(project.rootPath).canonicalFilePath();snapshot.revision=1;snapshot.sourceFiles={{"main.svml","source"}};snapshot.sourceFingerprint=QCryptographicHash::hash(QJsonDocument(QJsonObject{{"main.svml","source"}}).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);snapshot.space={640,360,60,2,30};services::ExportWorkspace::freeze(project,snapshot,&frozen,&error);task.workspace=frozen.workspace;task.buildId="bld_finished";task.phase="complete";task.revision=1;task.sourceFingerprint=snapshot.sourceFingerprint;task.space=snapshot.space;task.destination=temp.filePath("result.mp4");task.hypitVersion="0.2.10";services::ExportWorkspace::saveTask(project,task,&error);config.expectedHypitVersion="0.2.10";config.distributionPath=temp.filePath("distribution");config.launcherPath=config.distributionPath+"/hypit";config.processEnvironment.insert("QVW_CACHE_TEST","configured");write(config.launcherPath,R"PY(#!/usr/bin/python3
import json,sys,pathlib,os
root=pathlib.Path(__file__).parent
mode=(root/'mode').read_text() if (root/'mode').exists() else 'success'
cmd=sys.argv[1]
with (root/'calls').open('a') as f:f.write(json.dumps(sys.argv[1:])+'\n')
if os.environ.get('QVW_CACHE_TEST')!='configured':print('{"format":"hypit.cli-error@1"}');sys.exit(2)
if cmd=='status':
 b={'id':'bld_wrong' if mode=='wrongid' else 'bld_finished','work':{'state':'working' if mode=='active' else 'done','outcome':'failed' if mode=='failed' else 'cancelled' if mode=='cancelled' else 'complete'},'result':{'state':'failed' if mode=='failed' else 'cancelled' if mode=='cancelled' else 'missing' if mode=='missing' else 'complete'}}
 if mode=='attention':b['attention']={'message':'unknown'}
 print(json.dumps({'format':'hypit.cli-status@1','build':b}));sys.exit(1 if mode=='failed' else 0)
elif cmd=='activity':
 a={'format':'hypit.cli-activity@1','worker':'running','builds':[]}
 if mode=='otheractive':a['builds']=[{'id':'bld_other','work':{'state':'working'},'phases':{}}]
 if mode=='currentactive':a['builds']=[{'id':'bld_finished','work':{'state':'working'},'phases':{}}]
 if mode=='otherdone':a['builds']=[{'id':'bld_other','work':{'state':'done','outcome':'complete'},'phases':{}}]
 if mode=='omitted':a['omittedBuilds']=1
 if mode=='malformed':a['builds']={}
 print(json.dumps(a))
elif cmd=='runtime':print(json.dumps({'format':'hypit.cli-runtime-down@1','worker':'running' if mode=='downfail' else 'stopped'}));sys.exit(1 if mode=='downfail' else 0)
)PY");QFile::setPermissions(config.launcherPath,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);}
 void mode(const QByteArray &mode){write(config.distributionPath+"/mode",mode);}
 int calls(const QString &command)const{int n=0;for(const auto &line:read(config.distributionPath+"/calls").split('\n')){const auto args=QJsonDocument::fromJson(line).array();if(!args.isEmpty()&&args[0].toString()==command)++n;}return n;}
 bool save(){return services::ExportWorkspace::saveTask(project,task,&error);}
};
bool clear(controllers::ExportController &controller){return QMetaObject::invokeMethod(&controller,"clearFinishedCache",Qt::DirectConnection);}
}
class ExportCacheTest:public QObject {
 Q_OBJECT
private slots:
 void clearsOnlyKnownTerminalWorkspace_data(){QTest::addColumn<QByteArray>("mode");QTest::newRow("complete")<<QByteArray("success");QTest::newRow("failed")<<QByteArray("failed");QTest::newRow("cancelled")<<QByteArray("cancelled");}
 void clearsOnlyKnownTerminalWorkspace(){QFETCH(QByteArray,mode);Fixture f;QVERIFY2(f.error.isEmpty(),qPrintable(f.error));f.mode(mode);f.task.phase=mode=="success"?"complete":QString::fromUtf8(mode);QVERIFY(f.save());services::FrozenExport other;QVERIFY(services::ExportWorkspace::freeze(f.project,f.snapshot,&other,&f.error));infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);c.setProject(f.project);QSignalSpy failed(&c,&controllers::ExportController::failed);QVERIFY(clear(c));QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(f.frozen.workspace),3000);QCOMPARE(failed.size(),0);QCOMPARE(f.calls("status"),1);QCOMPARE(f.calls("activity"),1);QCOMPARE(f.calls("runtime"),1);QVERIFY(!QFileInfo::exists(f.project.rootPath+"/.workbench/export-task.json"));QVERIFY(QFileInfo(other.workspace).isDir());QCOMPARE(read(f.project.rootPath+"/main.svml"),QByteArray("source"));QCOMPARE(read(f.project.rootPath+"/assets/image.png"),QByteArray("asset"));QCOMPARE(c.task().phase,QString("idle"));QVERIFY(!c.isBusy());}
 void refusesUnknownOrActiveRecord_data(){QTest::addColumn<QString>("phase");QTest::addColumn<bool>("active");QTest::newRow("working")<<QString("working")<<true;QTest::newRow("stopped")<<QString("stopped")<<false;QTest::newRow("getting")<<QString("getting")<<false;QTest::newRow("validating")<<QString("validating")<<false;QTest::newRow("active complete")<<QString("complete")<<true;}
 void refusesUnknownOrActiveRecord(){QFETCH(QString,phase);QFETCH(bool,active);Fixture f;f.task.phase=phase;f.task.active=active;QVERIFY(f.save());infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);c.setProject(f.project);QSignalSpy failed(&c,&controllers::ExportController::failed);QVERIFY(clear(c));QCOMPARE(failed.size(),1);QCOMPARE(f.calls("status"),0);QVERIFY(QFileInfo::exists(f.frozen.workspace));}
 void refusesUnconfirmedBackend_data(){QTest::addColumn<QByteArray>("mode");for(const auto &mode:{"active","wrongid","missing","attention","otheractive","currentactive","otherdone","omitted","malformed","downfail"})QTest::newRow(mode)<<QByteArray(mode);}
 void refusesUnconfirmedBackend(){QFETCH(QByteArray,mode);Fixture f;f.mode(mode);infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);c.setProject(f.project);QSignalSpy failed(&c,&controllers::ExportController::failed);QVERIFY(clear(c));QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,3000);QVERIFY(QFileInfo::exists(f.frozen.workspace));QVERIFY(QFileInfo::exists(f.project.rootPath+"/.workbench/export-task.json"));if(mode!="downfail")QCOMPARE(f.calls("runtime"),0);QCOMPARE(c.task().phase,QString("complete"));QVERIFY(!c.isBusy());}
 void refusesSymlinkInsideRuntime(){Fixture f;QTemporaryDir outside;QVERIFY(QFile::link(outside.path(),f.frozen.workspace+"/.hypit"));infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);c.setProject(f.project);QSignalSpy failed(&c,&controllers::ExportController::failed);QVERIFY(clear(c));QCOMPARE(failed.size(),1);QCOMPARE(f.calls("status"),0);QVERIFY(QFileInfo::exists(f.frozen.workspace));QVERIFY(QFileInfo(outside.path()).isDir());}
 void rejectsTamperedMarker(){Fixture f;write(f.frozen.workspace+"/.frozen-inputs.json","{}");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);c.setProject(f.project);QSignalSpy failed(&c,&controllers::ExportController::failed);QVERIFY(clear(c));QCOMPARE(failed.size(),1);QCOMPARE(f.calls("status"),0);QVERIFY(QFileInfo::exists(f.frozen.workspace));}
 void rejectsUnknownRecordPhase(){Fixture f;f.task.phase="invented-terminal";QVERIFY(f.save());domain::ExportTask loaded;QVERIFY(!services::ExportWorkspace::loadTask(f.project,&loaded,&f.error));QVERIFY(!f.error.isEmpty());}
 void refusesEmptyBuildId(){Fixture f;f.task.buildId.clear();f.task.phase="failed";QVERIFY(f.save());infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);c.setProject(f.project);QSignalSpy failed(&c,&controllers::ExportController::failed);QVERIFY(clear(c));QCOMPARE(failed.size(),1);QCOMPARE(f.calls("runtime"),0);QVERIFY(QFileInfo::exists(f.frozen.workspace));}
 void autoResolvedNodePreservesOtherExecutableCli(){Fixture f;const auto node=f.temp.filePath("fake-tools/node");QVERIFY(write(node,"#!/bin/sh\nexit 65\n"));QVERIFY(QFile::setPermissions(node,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));const auto path=f.temp.filePath("repo/config/version-lock.json");QVERIFY(write(path,"{\"hypit\":{\"distributionPath\":\"../distribution\",\"launcher\":\"hypit\",\"version\":\"0.2.10\"}}"));const auto original=qgetenv("PATH");qputenv("PATH",QFileInfo(node).absolutePath().toUtf8());const auto loaded=infra::loadAppConfig(path);qputenv("PATH",original);QVERIFY2(loaded.ok(),qPrintable(loaded.error));auto config=loaded.config;config.processEnvironment.insert("QVW_CACHE_TEST","configured");infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(config,log);c.setProject(f.project);QSignalSpy failed(&c,&controllers::ExportController::failed);QVERIFY(clear(c));QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(f.frozen.workspace),3000);QCOMPARE(failed.size(),0);}
 void explicitNodeExecutableRunsCli(){Fixture f;const auto node=f.temp.filePath("custom-node");QVERIFY(write(node,"#!/bin/sh\nexec /usr/bin/python3 \"$@\"\n"));QVERIFY(QFile::setPermissions(node,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));auto script=read(f.config.launcherPath);script.replace("#!/usr/bin/python3","#!/does/not/exist");QVERIFY(write(f.config.launcherPath,script));const auto javascriptLauncher=f.config.launcherPath+".mjs";QVERIFY(QFile::rename(f.config.launcherPath,javascriptLauncher));f.config.launcherPath=javascriptLauncher;f.config.nodePath=node;f.config.nodeExplicit=true;infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);c.setProject(f.project);QSignalSpy failed(&c,&controllers::ExportController::failed);QVERIFY(clear(c));QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(f.frozen.workspace),3000);QCOMPARE(failed.size(),0);}
 void projectSwitchDuringBusySignalCannotDeleteOldCache(){Fixture f,other;infra::LogWriter log(f.temp.filePath("log"));controllers::ExportController c(f.config,log);c.setProject(f.project);connect(&c,&controllers::ExportController::busyChanged,&c,[&](bool busy){if(busy)c.setProject(other.project);});QVERIFY(clear(c));QTest::qWait(100);QCOMPARE(f.calls("status"),0);QVERIFY(QFileInfo::exists(f.frozen.workspace));QVERIFY(QFileInfo::exists(other.frozen.workspace));QCOMPARE(c.task().workspace,other.frozen.workspace);QVERIFY(!c.isBusy());}
};
QTEST_GUILESS_MAIN(ExportCacheTest)
#include "ExportCacheTest.moc"
