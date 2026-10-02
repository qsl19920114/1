#include "services/SampleCatalog.h"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
class SampleCatalogTest:public QObject {
    Q_OBJECT
private slots:
    void missingDistributionHasNoSamples(){QVERIFY(qvw::services::SampleCatalog::discover("/nonexistent/qvw-distribution").isEmpty());}
    void duplicateOutputsCollapseAndSourcesStayVisible(){
        QTemporaryDir dir;QVERIFY(dir.isValid());
        const QStringList files{"output/chat-demo.mp4","examples/semantic-composition/.hypit/results/2026-09-20/bld_20260920T071529859Z_F3FD835600/files/file-0001.mp4"};
        for(const auto &path:files){QVERIFY(QDir().mkpath(QFileInfo(dir.filePath(path)).absolutePath()));QFile file(dir.filePath(path));QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write("same sample bytes"),17);}
        const auto samples=qvw::services::SampleCatalog::discover(dir.path());QCOMPARE(samples.size(),1);QCOMPARE(samples.front().sources.size(),2);QCOMPARE(samples.front().sha256.size(),64);
    }
    void symlinkOutsideDistributionIsIgnored(){
        QTemporaryDir dir,outside;QVERIFY(dir.isValid());QVERIFY(outside.isValid());QFile file(outside.filePath("video.mp4"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("video");file.close();QVERIFY(QDir().mkpath(dir.filePath("output")));QVERIFY(QFile::link(file.fileName(),dir.filePath("output/chat-demo.mp4")));const auto result=qvw::services::SampleCatalog::discover(dir.path());QCOMPARE(result.size(),1);QVERIFY(!result.front().available);QVERIFY(result.front().error.contains("路径不安全"));
    }
};
QTEST_GUILESS_MAIN(SampleCatalogTest)
#include "SampleCatalogTest.moc"
