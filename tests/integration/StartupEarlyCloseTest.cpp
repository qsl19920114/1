// Compile the production entry point into a small harness. A Qt startup hook
// closes its real window before the deliberately slow version probe finishes.
#define main qvwApplicationMain
#include "../../app/main.cpp"
#undef main
#include <QTemporaryDir>
#include <vector>

static void closeVerificationWindow() {
    QTimer::singleShot(100, [] {
        for (auto *widget : QApplication::topLevelWidgets())
            if (widget->isWindow() && widget->isVisible()) widget->close();
    });
}
Q_COREAPP_STARTUP_FUNCTION(closeVerificationWindow)

int main() {
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 1;
    const auto root=temporary.path();
    QDir().mkpath(root+"/config");QDir().mkpath(root+"/runtime/bin");
    auto write=[](const QString &path,const QByteArray &bytes,bool executable=false){QFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size())return false;file.close();return !executable||file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner);};
    const auto slowNode=root+"/slow-node";
    if(!write(slowNode,"#!/bin/sh\nexec /bin/sleep 5\n",true)
        ||!write(root+"/runtime/bin/hypit.mjs","// probe intentionally never completes\n",true))return 1;
    const QJsonObject config{{"hypit",QJsonObject{{"distributionPath","runtime"},{"launcher","bin/hypit.mjs"},{"version","0.2.10"}}},{"tools",QJsonObject{{"node",slowNode},{"ffmpeg","/usr/bin/true"},{"ffprobe","/usr/bin/true"}}}};
    if(!write(root+"/config/version-lock.json",QJsonDocument(config).toJson()))return 1;
    const auto report=root+"/report.json";
    QList<QByteArray> arguments{"early-close", "--verify-startup", "--create-project", (root+"/project").toUtf8(), "--config",(root+"/config/version-lock.json").toUtf8(),"--report-out",report.toUtf8(),"--log",(root+"/log.jsonl").toUtf8()};
    std::vector<char*> argv;for(auto &argument:arguments)argv.push_back(argument.data());argv.push_back(nullptr);
    const int code=qvwApplicationMain(int(arguments.size()),argv.data());
    QFile file(report);if(!file.open(QIODevice::ReadOnly))return 1;
    const auto result=QJsonDocument::fromJson(file.readAll()).object();
    if(code!=9||result["exitCode"]!=9||result["verdict"]!="FAIL"||result["error"].toString().isEmpty()
        ||result["snapshot"]!=false||result["compiledCompositionReady"]!=false
        ||result["imagesReady"]!=false||result["verificationSucceeded"]!=false||result["cleanupStopped"]!=true){qCritical()<<"Early close falsely accepted:"<<code<<result;return 1;}
    qInfo()<<"PASS: early close exits nonzero with incomplete FAIL report";
    return 0;
}
