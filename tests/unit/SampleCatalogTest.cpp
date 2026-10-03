#include "services/SampleCatalog.h"
#include "infrastructure/AppConfig.h"
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
class SampleCatalogTest:public QObject {
    Q_OBJECT
    static QJsonObject entry(const QString &path, const QByteArray &bytes = "official sample") {
        return {{"name","Official example"},{"path",path},
                {"sha256",QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())},
                {"sizeBytes",bytes.size()},{"durationSeconds",2.5},{"width",1280},{"height",720},
                {"sourceUrl","https://storage.googleapis.com/hypit-public-assets/assets/examples/final.mp4"}};
    }
    static bool write(const QString &path, const QByteArray &bytes) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size();
    }
    static QString catalog(const QTemporaryDir &dir, const QJsonArray &entries) {
        const auto path=dir.filePath("catalog.json");
        write(path,QJsonDocument(QJsonObject{{"schemaVersion",1},{"samples",entries}}).toJson());return path;
    }
private slots:
    void optionalCatalogConfigResolvesFromRepositoryRoot() {
        QTemporaryDir dir;QVERIFY(dir.isValid());
        const auto path=dir.filePath("config/version-lock.json");
        QJsonObject object{{"hypit",QJsonObject{{"distributionPath","../hypit"},{"launcher","bin/hypit"},{"version","0.2.10"}}},
                           {"samples",QJsonObject{{"catalogPath",".workbench/showcase/catalog.json"}}}};
        QVERIFY(write(path,QJsonDocument(object).toJson()));
        auto loaded=qvw::infra::loadAppConfig(path);QVERIFY2(loaded.ok(),qPrintable(loaded.error));
        QCOMPARE(loaded.config.sampleCatalogPath,dir.filePath(".workbench/showcase/catalog.json"));
        object["samples"]=QJsonObject{{"catalogPath",42}};
        QVERIFY(write(path,QJsonDocument(object).toJson()));
        QVERIFY(!qvw::infra::loadAppConfig(path).ok());
        object.remove("samples");QVERIFY(write(path,QJsonDocument(object).toJson()));
        loaded=qvw::infra::loadAppConfig(path);QVERIFY(loaded.ok());QVERIFY(loaded.config.sampleCatalogPath.isEmpty());
    }
    void catalogWorksWithoutDistributionAndDeduplicatesAcrossSources() {
        QTemporaryDir dir;QVERIFY(dir.isValid());
        QVERIFY(write(dir.filePath("one.mp4"),"official sample"));
        QVERIFY(write(dir.filePath("two.mp4"),"official sample"));
        const auto result=qvw::services::SampleCatalog::discover("/nonexistent/qvw",catalog(dir,{entry("one.mp4"),entry("two.mp4")}));
        QCOMPARE(result.size(),1);QVERIFY(result.front().available);
        QCOMPARE(result.front().name,QString("Official example"));
        QVERIFY(result.front().sources.join("\n").contains("https://storage.googleapis.com/"));
        QVERIFY(result.front().sources.join("\n").contains("two.mp4"));
        QVERIFY(write(dir.filePath("output/chat-demo.mp4"),"official sample"));
        const auto merged=qvw::services::SampleCatalog::discover(dir.path(),dir.filePath("catalog.json"));
        QCOMPARE(merged.size(),1);QVERIFY(merged.front().sources.contains("output/chat-demo.mp4"));
    }
    void invalidCatalogEntriesAreRejectedAndExplained() {
        QTemporaryDir dir,outside;QVERIFY(dir.isValid());QVERIFY(outside.isValid());
        QVERIFY(write(dir.filePath("valid.mp4"),"official sample"));
        QVERIFY(write(outside.filePath("outside.mp4"),"official sample"));
        QVERIFY(QFile::link(outside.filePath("outside.mp4"),dir.filePath("link.mp4")));
        QVERIFY(QFile::link(outside.path(),dir.filePath("linked-dir")));
        auto badHash=entry("valid.mp4");badHash["sha256"]=QString(64,'0');
        auto badMetadata=entry("valid.mp4");badMetadata["durationSeconds"]=-1;
        const auto result=qvw::services::SampleCatalog::discover("/nonexistent/qvw",catalog(dir,
            {entry("missing.mp4"),entry("../outside.mp4"),entry(outside.filePath("outside.mp4")),entry("link.mp4"),entry("linked-dir/outside.mp4"),badHash,badMetadata}));
        QCOMPARE(result.size(),1);QVERIFY(!result.front().available);QVERIFY(!result.front().error.isEmpty());
    }
    void malformedOrOversizedCatalogDoesNotHideFallback() {
        QTemporaryDir dir;QVERIFY(dir.isValid());QVERIFY(write(dir.filePath("output/chat-demo.mp4"),"local"));
        const auto path=dir.filePath("catalog.json");QVERIFY(write(path,"invalid json"));
        auto result=qvw::services::SampleCatalog::discover(dir.path(),path);
        QCOMPARE(result.size(),1);QVERIFY(result.front().available);QVERIFY(result.front().error.contains("样例目录格式无效"));
        QVERIFY(write(path,QByteArray(256*1024+1,' ')));
        result=qvw::services::SampleCatalog::discover(dir.path(),path);
        QCOMPARE(result.size(),1);QVERIFY(result.front().available);QVERIFY(result.front().error.contains("256KiB"));
    }
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
