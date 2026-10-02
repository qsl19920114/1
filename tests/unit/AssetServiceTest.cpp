#include "services/AssetService.h"
#include "services/ProjectStore.h"
#include <QtTest>
#include <QBuffer>
#include <QCryptographicHash>
#include <QImage>
#include <QTemporaryDir>
using qvw::domain::Project;
using qvw::domain::Asset;
using qvw::services::AssetService;
using qvw::services::ProjectStore;
class AssetServiceTest : public QObject {
    Q_OBJECT
    static bool write(const QString &path, const QByteArray &bytes) {
        QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    }
    static QByteArray read(const QString &path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
    static QByteArray image(const char *format, QRgb color = qRgb(80, 120, 180), QSize size = {31, 19}) {
        QImage bitmap(size, QImage::Format_RGB32); bitmap.fill(color);
        QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); bitmap.save(&buffer, format); return bytes;
    }
    static Project project(const QString &root) {
        Project p; p.rootPath = root + "/工程"; p.name = "Test"; p.templateId = "title"; p.templateVersion = "1";
        p.sourcePath = "title.sv"; p.runPath = "title.svrun"; p.runtimePath = "runtime.json";
        QDir().mkpath(p.rootPath);
        for (const auto &path : {p.sourcePath, p.runPath, p.runtimePath}) write(p.rootPath + '/' + path, "{}");
        QString error; if (!ProjectStore::save(p, &error)) qFatal("fixture: %s", qPrintable(error)); return p;
    }
private slots:
    void importsByContents_data() {
        QTest::addColumn<QByteArray>("format"); QTest::addColumn<QString>("filename"); QTest::addColumn<QString>("extension");
        QTest::newRow("png") << QByteArray("PNG") << QString("中文 图片.png") << QString("png");
        QTest::newRow("jpeg") << QByteArray("JPEG") << QString("photo.jpeg") << QString("jpg");
        QTest::newRow("misleading-png") << QByteArray("PNG") << QString("photo.jpg") << QString("png");
        QTest::newRow("misleading-jpeg") << QByteArray("JPEG") << QString("photo.png") << QString("jpg");
    }
    void importsByContents() {
        QFETCH(QByteArray, format); QFETCH(QString, filename); QFETCH(QString, extension);
        QTemporaryDir dir; auto p = project(dir.path()); const auto source = dir.path() + '/' + filename; const auto bytes = image(format);
        QVERIFY(!bytes.isEmpty()); QVERIFY(write(source, bytes)); Asset asset; QString error;
        QVERIFY2(AssetService::importImage(p, source, &asset, &error), qPrintable(error));
        QCOMPARE(asset.hash, QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()));
        QCOMPARE(asset.path, "assets/" + asset.hash + '.' + extension); QCOMPARE(asset.originalName, filename);
        QCOMPARE(asset.mime, extension == "png" ? QString("image/png") : QString("image/jpeg"));
        QCOMPARE(asset.width, 31); QCOMPARE(asset.height, 19); QCOMPARE(asset.size, bytes.size());
        QCOMPARE(read(source), bytes); QCOMPARE(read(p.rootPath + '/' + asset.path), bytes); QCOMPARE(p.assets.size(), 1);
        Project loaded; QVERIFY2(ProjectStore::load(p.manifestPath(), &loaded, &error), qPrintable(error));
        QCOMPARE(loaded.assets[0].path, asset.path);
    }
    void duplicatesAndChangedOriginal() {
        QTemporaryDir dir; auto p = project(dir.path()); const auto source = dir.path() + "/external.png";
        const auto original = image("PNG"); QVERIFY(write(source, original)); Asset first, duplicate, changed; QString error;
        QVERIFY(AssetService::importImage(p, source, &first, &error));
        const auto alias = dir.path() + "/another-name.jpg"; QVERIFY(write(alias, original));
        QVERIFY(AssetService::importImage(p, alias, &duplicate, &error)); QCOMPARE(duplicate.path, first.path); QCOMPARE(p.assets.size(), 1);
        QVERIFY(write(source, image("PNG", qRgb(1, 2, 3))));
        QVERIFY(AssetService::importImage(p, source, &changed, &error)); QCOMPARE(p.assets.size(), 2); QVERIFY(changed.path != first.path);
        QCOMPARE(read(p.rootPath + '/' + first.path), original);
    }
    void rejectsInvalid_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("garbage") << QByteArray("pretend png");
        QTest::newRow("unsupported-bmp") << image("BMP");
        QTest::newRow("truncated-png") << image("PNG").chopped(12);
        QTest::newRow("truncated-jpeg") << image("JPEG").chopped(2);
    }
    void rejectsInvalid() {
        QFETCH(QByteArray, bytes); QTemporaryDir dir; auto p = project(dir.path()); const auto source = dir.path() + "/bad.png";
        const auto before = read(p.manifestPath()); QVERIFY(write(source, bytes)); Asset asset; asset.path = "unchanged"; QString error;
        QVERIFY(!AssetService::importImage(p, source, &asset, &error)); QVERIFY(!error.isEmpty());
        QCOMPARE(asset.path, QString("unchanged")); QVERIFY(p.assets.isEmpty()); QCOMPARE(read(p.manifestPath()), before);
    }
    void enforcesByteAndPixelLimits() {
        QTemporaryDir dir; auto p = project(dir.path()); const auto source = dir.path() + "/image.png"; const auto bytes = image("PNG");
        QVERIFY(write(source, bytes)); p.maxAssetBytes = bytes.size() - 1; QString error;
        QVERIFY(!AssetService::importImage(p, source, nullptr, &error)); QVERIFY(p.assets.isEmpty());
        p.maxAssetBytes = 64 * 1024 * 1024;
        // A valid compressed 40,006,000-pixel image is small on disk, but exceeds the decoded pixel budget.
        QVERIFY(write(source, image("PNG", qRgb(0, 0, 0), {8000, 5001})));
        QVERIFY(!AssetService::importImage(p, source, nullptr, &error)); QVERIFY(error.contains("像素")); QVERIFY(p.assets.isEmpty());
    }
    void metadataFailureRollsBackOnlyNewCopy() {
        QTemporaryDir dir; auto p = project(dir.path()); const auto source = dir.path() + "/image.png"; const auto bytes = image("PNG");
        QVERIFY(write(source, bytes)); const auto future = QByteArray(R"({"format":"future@2"})"); QVERIFY(write(p.manifestPath(), future));
        QString error; QVERIFY(!AssetService::importImage(p, source, nullptr, &error)); QVERIFY(p.assets.isEmpty());
        QCOMPARE(read(p.manifestPath()), future); QCOMPARE(read(source), bytes);
        const auto target = p.rootPath + "/assets/" + QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() + ".png";
        QVERIFY(!QFile::exists(target)); QVERIFY(QDir().mkpath(p.rootPath + "/assets")); QVERIFY(write(target, bytes));
        QVERIFY(!AssetService::importImage(p, source, nullptr, &error)); QCOMPARE(read(target), bytes);
    }
    void manifestWriteFailurePreservesProject() {
        QTemporaryDir dir; auto p = project(dir.path()); const auto source = dir.path() + "/image.png"; const auto bytes = image("PNG");
        QVERIFY(write(source, bytes)); QVERIFY(QDir().mkpath(p.rootPath + "/assets")); const auto before = read(p.manifestPath());
        const auto permissions = QFileInfo(p.rootPath).permissions();
        QVERIFY(QFile::setPermissions(p.rootPath, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        QString error; Asset output; output.path = "unchanged";
        const auto imported = AssetService::importImage(p, source, &output, &error);
        QVERIFY(QFile::setPermissions(p.rootPath, permissions));
        QVERIFY(!imported); QVERIFY(!error.isEmpty()); QVERIFY(p.assets.isEmpty()); QCOMPARE(output.path, QString("unchanged"));
        QCOMPARE(read(p.manifestPath()), before); QCOMPARE(read(source), bytes);
        QVERIFY(QDir(p.rootPath + "/assets").entryList(QDir::Files).isEmpty());
    }
    void registeredCorruptionIsNotSilentlyDeduplicated() {
        QTemporaryDir dir; auto p = project(dir.path()); const auto source = dir.path() + "/image.png"; const auto bytes = image("PNG");
        QVERIFY(write(source, bytes)); Asset first; QString error; QVERIFY(AssetService::importImage(p, source, &first, &error));
        const auto before = read(p.manifestPath()); QVERIFY(write(p.rootPath + '/' + first.path, "corrupt"));
        QVERIFY(!AssetService::importImage(p, source, nullptr, &error)); QCOMPARE(p.assets.size(), 1);
        QCOMPARE(read(p.manifestPath()), before); QCOMPARE(read(p.rootPath + '/' + first.path), QByteArray("corrupt"));
    }
    void refusesCorruptExistingTarget() {
        QTemporaryDir dir; auto p = project(dir.path()); const auto source = dir.path() + "/image.png"; const auto bytes = image("PNG");
        QVERIFY(write(source, bytes)); QVERIFY(QDir().mkpath(p.rootPath + "/assets"));
        const auto target = p.rootPath + "/assets/" + QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() + ".png";
        QVERIFY(write(target, "keep existing corruption")); QString error;
        QVERIFY(!AssetService::importImage(p, source, nullptr, &error)); QVERIFY(p.assets.isEmpty()); QCOMPARE(read(target), QByteArray("keep existing corruption"));
    }
    void rejectsEscapingAssetsLink() {
        QTemporaryDir dir; auto p = project(dir.path()); const auto outside = dir.path() + "/outside"; QVERIFY(QDir().mkdir(outside));
        QVERIFY(QFile::link(outside, p.rootPath + "/assets")); const auto source = dir.path() + "/image.png"; QVERIFY(write(source, image("PNG")));
        QString error; QVERIFY(!AssetService::importImage(p, source, nullptr, &error)); QVERIFY(p.assets.isEmpty());
        QVERIFY(QDir(outside).entryList(QDir::Files).isEmpty());
    }
};
QTEST_GUILESS_MAIN(AssetServiceTest)
#include "AssetServiceTest.moc"
