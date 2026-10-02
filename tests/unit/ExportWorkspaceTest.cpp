#include "services/ExportWorkspace.h"
#include "services/ProjectStore.h"
#include <QtTest>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QFile>
using namespace qvw;
namespace {
bool write(const QString &path,const QByteArray &bytes){QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);return f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size();}
QByteArray read(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
domain::Project project(const QString &root) {
    write(root+"/main.svml","source");write(root+"/main.svrun","run");write(root+"/packages/custom/index.js","package");write(root+"/assets/image.png","asset");write(root+"/.hypit/state","old worker");
    write(root+"/hypit.runtime.json",R"({"format":"hypit.runtime-local@1","dataRoot":".hypit/runtimes/local","endpoints":{"media.local":{"use":"@hypit/provider-media-local","config":{}}}})");
    domain::Project p;p.rootPath=root;p.sourcePath="main.svml";p.runPath="main.svrun";p.runtimePath="hypit.runtime.json";return p;
}
domain::Snapshot snapshot(){domain::Snapshot s;s.revision=7;s.sourceFiles={{"main.svml","source"}};QJsonObject a{{"main.svml","source"}};s.sourceFingerprint=QCryptographicHash::hash(QJsonDocument(a).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);s.space={640,360,60,2,30};return s;}
}
class ExportWorkspaceTest:public QObject {
 Q_OBJECT
private slots:
 void freezesIndependentFiles() {
    QTemporaryDir dir;auto p=project(dir.path());services::FrozenExport frozen;QString error;QVERIFY2(services::ExportWorkspace::freeze(p,snapshot(),&frozen,&error),qPrintable(error));
    QVERIFY(frozen.workspace.startsWith(QFileInfo(dir.path()).canonicalFilePath()+"/.workbench/builds/"));QCOMPARE(read(frozen.workspace+"/main.svml"),QByteArray("source"));
    QCOMPARE(read(frozen.workspace+"/packages/custom/index.js"),QByteArray("package"));QCOMPARE(read(frozen.workspace+"/assets/image.png"),QByteArray("asset"));
    QVERIFY(!QFileInfo::exists(frozen.workspace+"/.hypit/state"));write(dir.path()+"/main.svml","edited");QCOMPARE(read(frozen.workspace+"/main.svml"),QByteArray("source"));
 }
 void localRuntimeAcceptedAndPaidRejected() {
    QTemporaryDir dir;auto p=project(dir.path());QString error;QVERIFY(services::ExportWorkspace::validateLocalRuntime(dir.filePath(p.runtimePath),&error));
    write(dir.filePath(p.runtimePath),R"({"format":"hypit.runtime-local@1","dataRoot":".hypit/local","endpoints":{"paid":{"use":"@vendor/paid","config":{}}}})");QVERIFY(!services::ExportWorkspace::validateLocalRuntime(dir.filePath(p.runtimePath),&error));
 }
 void rejectsExternalSourceAndLinks() {
    QTemporaryDir dir;auto p=project(dir.path());auto s=snapshot();services::FrozenExport frozen;QString error;write(dir.filePath("main.svml"),"external");QVERIFY(!services::ExportWorkspace::freeze(p,s,&frozen,&error));
    write(dir.filePath("main.svml"),"source");QVERIFY(QFile::link(dir.filePath("main.svml"),dir.filePath("linked.svml")));QVERIFY(!services::ExportWorkspace::freeze(p,s,&frozen,&error));
 }
 void taskRoundtripAndRecoveredContainment() {
    QTemporaryDir dir;auto p=project(dir.path());auto s=snapshot();services::FrozenExport frozen;QString error;QVERIFY(services::ExportWorkspace::freeze(p,s,&frozen,&error));
    domain::ExportTask t;t.phase="working";t.buildId="bld_test_123";t.workspace=frozen.workspace;t.revision=s.revision;t.sourceFingerprint=s.sourceFingerprint;t.space=s.space;t.destination=dir.filePath("result.mp4");t.active=true;
    QVERIFY2(services::ExportWorkspace::saveTask(p,t,&error),qPrintable(error));domain::ExportTask loaded;QVERIFY2(services::ExportWorkspace::loadTask(p,&loaded,&error),qPrintable(error));
    QCOMPARE(loaded.buildId,t.buildId);QCOMPARE(loaded.revision,7);QVERIFY(loaded.active);QVERIFY(services::ExportWorkspace::validateRecovered(p,loaded,&frozen,&error));
    loaded.workspace="/private/tmp";QVERIFY(!services::ExportWorkspace::validateRecovered(p,loaded,&frozen,&error));
 }
 void rejectsAbsoluteRuntimeRoot() {
    QTemporaryDir dir;auto p=project(dir.path());QString error;write(dir.filePath(p.runtimePath),R"({"format":"hypit.runtime-local@1","dataRoot":"/private/tmp/shared","endpoints":{"media":{"use":"@hypit/provider-media-local","config":{}}}})");QVERIFY(!services::ExportWorkspace::validateLocalRuntime(dir.filePath(p.runtimePath),&error));
 }
 void recoveredInputsMustMatchFrozenFingerprint() {
    QTemporaryDir dir;auto p=project(dir.path());auto s=snapshot();services::FrozenExport frozen;QString error;QVERIFY(services::ExportWorkspace::freeze(p,s,&frozen,&error));
    domain::ExportTask t;t.workspace=frozen.workspace;t.buildId="bld_test_123";t.destination=dir.filePath("result.mp4");t.revision=s.revision;t.sourceFingerprint=s.sourceFingerprint;t.space=s.space;
    QVERIFY(services::ExportWorkspace::validateRecovered(p,t,&frozen,&error));write(frozen.workspace+"/main.svml","tampered");QVERIFY(!services::ExportWorkspace::validateRecovered(p,t,&frozen,&error));
 }
 void runtimeDataRootCannotEscapeBySymlink() {
    QTemporaryDir dir,outside;auto p=project(dir.path());QString error;QDir().mkpath(dir.filePath(".hypit/runtimes"));QVERIFY(QFile::link(outside.path(),dir.filePath(".hypit/runtimes/local")));QVERIFY(!services::ExportWorkspace::validateLocalRuntime(dir.filePath(p.runtimePath),&error));
 }
};
QTEST_GUILESS_MAIN(ExportWorkspaceTest)
#include "ExportWorkspaceTest.moc"
