#include "agent/ModelClient.h"
#include "agent/ContextBuilder.h"
#include "agent/PlanService.h"
#include <QCoreApplication>
#include <QSaveFile>
#include <QJsonDocument>
#include <QDebug>
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);qvw::agent::ModelClient client;
    QObject::connect(&client,&qvw::agent::ModelClient::failed,&app,[&](const QString &error){qCritical().noquote()<<error;app.exit(2);});
    QObject::connect(&client,&qvw::agent::ModelClient::completed,&app,[&](const QJsonObject &json){
        qvw::domain::AgentPlan plan;QString error;
        if(!qvw::agent::PlanService::parse(json,{},&plan,&error)||plan.kind()!="clarify"){qCritical().noquote()<<"Live model did not return required missing-assets clarification:"<<error;app.exit(3);return;}
        QSaveFile file(QStringLiteral(QVW_SOURCE_DIR)+"/docs/evidence/m8/real-model-probe.json");
        const auto bytes=QJsonDocument(QJsonObject{{"verdict","PASS"},{"provider","codex-current-chatgpt-login"},{"realModel",true},{"pixelsUploaded",false},{"response",json}}).toJson();
        if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit()){app.exit(4);return;}
        qInfo()<<"Real model structured clarification PASS";app.exit(0);
    });
    const auto context=qvw::agent::ContextBuilder::build({}, {},{},{});
    client.request(qvw::agent::ContextBuilder::prompt(QStringLiteral("用三张图片做15秒招新短片。我还没有选择素材，请指出需要我做什么。"),context),qvw::agent::PlanService::schema());
    return app.exec();
}
