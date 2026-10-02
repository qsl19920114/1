#include "services/MediaValidation.h"
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
using namespace qvw;
namespace {
QString script(const QString &path,const QString &body){QFile f(path);if(!f.open(QIODevice::WriteOnly))return {};f.write(("#!/usr/bin/python3\n"+body+"\n").toUtf8());f.close();f.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);return path;}
QString probe(const QString &path,const QString &codec="h264",int width=640,const QString &fps="30/1",double duration=2){return script(path,QString("print('{\"streams\":[{\"codec_type\":\"video\",\"codec_name\":\"%1\",\"width\":%2,\"height\":360,\"avg_frame_rate\":\"%3\"}],\"format\":{\"format_name\":\"mov,mp4,m4a,3gp,3g2,mj2\",\"duration\":\"%4\"}}')").arg(codec).arg(width).arg(fps).arg(duration));}
QString movie(QTemporaryDir &dir){QFile f(dir.filePath("stage.mp4"));if(!f.open(QIODevice::WriteOnly))return {};f.write("test resource");return f.fileName();}
}
class MediaValidationTest:public QObject {
 Q_OBJECT
private slots:
 void requiresProbeAndFullDecode() {
    QTemporaryDir dir;auto file=movie(dir);auto p=probe(dir.filePath("probe"));auto decode=script(dir.filePath("decode"),"import sys\nassert '-xerror' in sys.argv and 'null' in sys.argv\nprint('decoded')");
    services::MediaValidation v;QSignalSpy ok(&v,&services::MediaValidation::validated),failed(&v,&services::MediaValidation::failed),commands(&v,&services::MediaValidation::commandFinished);
    QVERIFY(v.start(file,{640,360,60,2,30},p,decode));QTRY_COMPARE_WITH_TIMEOUT(ok.size(),1,1500);QCOMPARE(failed.size(),0);QCOMPARE(commands.size(),2);
 }
 void rejectsMetadata_data(){QTest::addColumn<QString>("codec");QTest::addColumn<int>("width");QTest::addColumn<QString>("fps");QTest::addColumn<double>("duration");QTest::newRow("codec")<<QString("hevc")<<640<<QString("30/1")<<2.;QTest::newRow("size")<<QString("h264")<<1280<<QString("30/1")<<2.;QTest::newRow("fps")<<QString("h264")<<640<<QString("0/0")<<2.;QTest::newRow("duration")<<QString("h264")<<640<<QString("30/1")<<3.;}
 void rejectsMetadata() {
    QFETCH(QString,codec);QFETCH(int,width);QFETCH(QString,fps);QFETCH(double,duration);QTemporaryDir dir;services::MediaValidation v;QSignalSpy failed(&v,&services::MediaValidation::failed),ok(&v,&services::MediaValidation::validated);
    QVERIFY(v.start(movie(dir),{640,360,60,2,30},probe(dir.filePath("probe"),codec,width,fps,duration),script(dir.filePath("decode"),"pass")));QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,1000);QCOMPARE(ok.size(),0);
 }
 void decodeFailureNeverSucceeds() {
    QTemporaryDir dir;services::MediaValidation v;QSignalSpy failed(&v,&services::MediaValidation::failed),ok(&v,&services::MediaValidation::validated);
    QVERIFY(v.start(movie(dir),{640,360,60,2,30},probe(dir.filePath("probe")),script(dir.filePath("decode"),"import sys\nprint('corrupt frame',file=sys.stderr)\nsys.exit(1)")));QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,1000);QCOMPARE(ok.size(),0);
 }
 void missingToolIsReadableFailure(){QTemporaryDir dir;services::MediaValidation v;QSignalSpy failed(&v,&services::MediaValidation::failed);v.start(movie(dir),{640,360,60,2,30},"/missing/ffprobe","/missing/ffmpeg");QTRY_COMPARE_WITH_TIMEOUT(failed.size(),1,1000);QVERIFY(failed[0][0].toString().contains("ffprobe"));}
};
QTEST_GUILESS_MAIN(MediaValidationTest)
#include "MediaValidationTest.moc"
