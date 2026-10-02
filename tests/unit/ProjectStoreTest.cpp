#include "services/ProjectStore.h"
#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
using qvw::domain::Project;
using qvw::services::ProjectStore;
class ProjectStoreTest : public QObject {
    Q_OBJECT
    static bool write(const QString &path, const QByteArray &bytes) {
        QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    }
    static QByteArray read(const QString &path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; return f.readAll(); }
    static QString fixture(const QString &root) {
        const auto path = root + "/template";
        QDir().mkpath(path + "/source");
        write(path + "/template.json", R"({"format":"qt-video-workbench.template@1","id":"title-card","title":"标题","version":"1.0.0","source":"source/title.sv","run":"title.svrun","runtime":"hypit.runtime.json"})");
        write(path + "/source/title.sv", "authored source");
        write(path + "/title.svrun", "run"); write(path + "/hypit.runtime.json", "{}"); return path;
    }
private slots:
    void createReopenAndRelocate() {
        QTemporaryDir dir; const auto tpl = fixture(dir.path());
        Project project; QString error;
        QVERIFY2(ProjectStore::create(tpl, dir.path() + "/中文 工程", "我的标题", &project, &error), qPrintable(error));
        QCOMPARE(project.name, QString("我的标题")); QCOMPARE(project.templateId, QString("title-card"));
        QCOMPARE(project.sourcePath, QString("source/title.sv")); QCOMPARE(project.maxAssetBytes, qint64(64 * 1024 * 1024));
        QVERIFY(!QFile::exists(project.rootPath + "/template.json"));
        Project reopened; QVERIFY2(ProjectStore::load(project.manifestPath(), &reopened, &error), qPrintable(error));
        QCOMPARE(reopened.name, project.name);
        const auto renamed = dir.path() + "/搬迁 后"; QVERIFY(QDir().rename(project.rootPath, renamed));
        QVERIFY2(ProjectStore::load(renamed + "/workbench.qvw.json", &reopened, &error), qPrintable(error));
        QString absolute; QVERIFY(ProjectStore::resolvePath(reopened, reopened.sourcePath, &absolute, &error));
        QCOMPARE(read(absolute), QByteArray("authored source"));
        QCOMPARE(read(tpl + "/source/title.sv"), QByteArray("authored source"));
    }
    void noncanonicalManifestNameRejected() {
        QTemporaryDir dir; Project p; QString error;
        QVERIFY(ProjectStore::create(fixture(dir.path()), dir.path() + "/project", "Test", &p, &error));
        const auto other = p.rootPath + "/other.json"; QVERIFY(QFile::copy(p.manifestPath(), other));
        Project loaded; QVERIFY(!ProjectStore::load(other, &loaded, &error)); QVERIFY(!error.isEmpty());
    }
    void assetsRoundTrip() {
        QTemporaryDir dir; Project p; QString error;
        QVERIFY(ProjectStore::create(fixture(dir.path()), dir.path() + "/project", "Test", &p, &error));
        QDir().mkpath(p.rootPath + "/assets"); QVERIFY(write(p.rootPath + "/assets/a.png", "asset"));
        p.assets.push_back({QString(64, 'a'), "assets/a.png", "原图.png", "image/png", 20, 30, 5});
        p.maxAssetBytes = 1024; QVERIFY2(ProjectStore::save(p, &error), qPrintable(error));
        Project loaded; QVERIFY(ProjectStore::load(p.manifestPath(), &loaded, &error));
        QCOMPARE(loaded.assets.size(), 1); QCOMPARE(loaded.assets[0].originalName, QString("原图.png"));
        QCOMPARE(loaded.assets[0].width, 20); QCOMPARE(loaded.assets[0].height, 30); QCOMPARE(loaded.assets[0].size, qint64(5));
        QCOMPARE(loaded.maxAssetBytes, qint64(1024));
    }
    void invalidManifest_data() {
        QTest::addColumn<QByteArray>("content");
        QTest::newRow("invalid-json") << QByteArray("{");
        QTest::newRow("wrong-root") << QByteArray("[]");
        QTest::newRow("wrong-version") << QByteArray(R"({"format":"qt-video-workbench.project@99"})");
        QTest::newRow("missing-schema") << QByteArray(R"({"format":"qt-video-workbench.project@1"})");
    }
    void invalidManifest() {
        QFETCH(QByteArray, content); QTemporaryDir dir; const auto file = dir.path() + "/workbench.qvw.json"; QVERIFY(write(file, content));
        Project p; p.name = "untouched"; QString error; QVERIFY(!ProjectStore::load(file, &p, &error));
        QVERIFY(!error.isEmpty()); QCOMPARE(p.name, QString("untouched"));
    }
    void escapePathsAndSymlinks() {
        QTemporaryDir dir; Project p; QString error;
        QVERIFY(ProjectStore::create(fixture(dir.path()), dir.path() + "/project", "Test", &p, &error));
        QString path;
        for (const auto &bad : {QString("../escape"), QString("source/../../escape"), QString("/tmp/escape"), QString("C:\\escape"), QString("source\\escape"), QString(".")}) {
            QVERIFY2(!ProjectStore::resolvePath(p, bad, &path, &error, false), qPrintable(bad));
        }
        QVERIFY(write(dir.path() + "/outside", "secret"));
        QVERIFY(QFile::link(dir.path() + "/outside", p.rootPath + "/linked"));
        QVERIFY(!ProjectStore::resolvePath(p, "linked", &path, &error));
        QVERIFY(QFile::link(dir.path(), p.rootPath + "/linked-dir"));
        QVERIFY(!ProjectStore::resolvePath(p, "linked-dir/new-file", &path, &error, false));
    }
    void rejectedSavePreservesManifest() {
        QTemporaryDir dir; Project p; QString error;
        QVERIFY(ProjectStore::create(fixture(dir.path()), dir.path() + "/project", "Test", &p, &error));
        const auto before = read(p.manifestPath()); p.sourcePath = "../outside";
        QVERIFY(!ProjectStore::save(p, &error)); QCOMPARE(read(p.manifestPath()), before);
        p.sourcePath = "source/title.sv";
        QVERIFY(write(p.manifestPath(), R"({"format":"future@2"})"));
        QVERIFY(!ProjectStore::save(p, &error)); QCOMPARE(read(p.manifestPath()), QByteArray(R"({"format":"future@2"})"));
    }
    void malformedAssetsAndMissingSource() {
        QTemporaryDir dir; Project p; QString error;
        QVERIFY(ProjectStore::create(fixture(dir.path()), dir.path() + "/project", "Test", &p, &error));
        auto object = QJsonDocument::fromJson(read(p.manifestPath())).object();
        object["assets"] = QJsonArray{QJsonObject{{"hash", "bad"}, {"path", "source/title.sv"}, {"originalName", "x"}, {"mime", "image/png"}, {"width", 1}, {"height", 1}, {"size", 1}}};
        QVERIFY(write(p.manifestPath(), QJsonDocument(object).toJson()));
        Project loaded; QVERIFY(!ProjectStore::load(p.manifestPath(), &loaded, &error));
        object["assets"] = QJsonArray{}; QVERIFY(write(p.manifestPath(), QJsonDocument(object).toJson()));
        QVERIFY(QFile::remove(p.rootPath + "/source/title.sv"));
        QVERIFY(!ProjectStore::load(p.manifestPath(), &loaded, &error));
    }
    void nestedAndExistingEmptyDestination() {
        QTemporaryDir dir; const auto tpl = fixture(dir.path()); Project p; QString error;
        QVERIFY2(ProjectStore::create(tpl, dir.path() + "/nested/another/project", "Test", &p, &error), qPrintable(error));
        const auto empty = dir.path() + "/empty"; QVERIFY(QDir().mkdir(empty));
        QVERIFY2(ProjectStore::create(tpl, empty, "Test", &p, &error), qPrintable(error));
        QVERIFY(!ProjectStore::create(tpl, tpl + "/nested", "Test", &p, &error));
        QVERIFY(!QFile::exists(tpl + "/nested"));
    }
    void destinationSymlinkEscapeRejected() {
        QTemporaryDir dir; const auto tpl = fixture(dir.path()); Project p; QString error;
        QVERIFY(QFile::link(tpl, dir.path() + "/alias"));
        QVERIFY(!ProjectStore::create(tpl, dir.path() + "/alias/inside", "Test", &p, &error));
        QVERIFY(!QFile::exists(tpl + "/inside"));
    }
    void writeFailurePreservesPreviousManifest() {
        QTemporaryDir dir; Project p; QString error;
        QVERIFY(ProjectStore::create(fixture(dir.path()), dir.path() + "/project", "Test", &p, &error));
        const auto before = read(p.manifestPath()); const auto permissions = QFileInfo(p.rootPath).permissions();
        QVERIFY(QFile::setPermissions(p.rootPath, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
        p.name = "changed"; const auto saved = ProjectStore::save(p, &error);
        QVERIFY(QFile::setPermissions(p.rootPath, permissions));
        QVERIFY2(!saved, "Atomic save must fail when its directory is not writable");
        QCOMPARE(read(p.manifestPath()), before); QVERIFY(!error.isEmpty());
    }
    void failedCopyRollsBackOnlyCreatedFiles() {
        QTemporaryDir dir; const auto tpl = fixture(dir.path()); const auto blocked = tpl + "/blocked";
        QVERIFY(write(blocked, "private")); const auto permissions = QFileInfo(blocked).permissions();
        QVERIFY(QFile::setPermissions(blocked, QFileDevice::WriteOwner));
        const auto empty = dir.path() + "/empty"; QVERIFY(QDir().mkdir(empty));
        Project p; QString error; const auto created = ProjectStore::create(tpl, empty, "Test", &p, &error);
        QVERIFY(QFile::setPermissions(blocked, permissions));
        QVERIFY(!created); QVERIFY(QFileInfo(empty).isDir());
        QVERIFY(QDir(empty).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
        QCOMPARE(read(blocked), QByteArray("private"));
    }
    void destinationPreservedAndTemplateSymlinksRejected() {
        QTemporaryDir dir; const auto tpl = fixture(dir.path()); const auto destination = dir.path() + "/existing";
        QDir().mkpath(destination); QVERIFY(write(destination + "/mine", "keep"));
        Project p; QString error; QVERIFY(!ProjectStore::create(tpl, destination, "Test", &p, &error));
        QCOMPARE(read(destination + "/mine"), QByteArray("keep"));
        QVERIFY(QFile::link(tpl + "/title.svrun", tpl + "/link"));
        QVERIFY(!ProjectStore::create(tpl, dir.path() + "/new", "Test", &p, &error));
        QVERIFY(!QFile::exists(dir.path() + "/new/workbench.qvw.json"));
        QVERIFY(!QFile::exists(dir.path() + "/new/source/title.sv"));
    }
};
QTEST_GUILESS_MAIN(ProjectStoreTest)
#include "ProjectStoreTest.moc"
