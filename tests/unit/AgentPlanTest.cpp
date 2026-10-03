#include "agent/PlanService.h"
#include <QTest>
#include <QTemporaryDir>
#include <QJsonArray>

using namespace qvw;
class AgentPlanTest : public QObject {
    Q_OBJECT
    QJsonObject base() const {return {{"format","qvw.agent-plan@1"},{"kind","edit"},{"summary","只修改结尾"},{"question",""},{"projectName",""},{"scenes",QJsonArray{}},{"operations",QJsonArray{QJsonObject{{"entityId","ending"},{"fieldId","title"},{"value","周五一起玩！"}}}}};}
    domain::Snapshot snapshot() const {domain::Snapshot s;s.revision=17;s.sourceFingerprint=QByteArray(32,'a');s.sourcePath="main.svml";s.sourceFiles.insert("main.svml","source");domain::InspectorField f;f.id="title";f.binding="title";f.control=domain::ControlKind::Text;f.rawValue="周五晚七点";f.writable=true;domain::Clip c;c.id="ending";c.inspector={f};domain::Track t;t.id="story";t.clips={c};s.tracks={t};return s;}
private slots:
    void approvedScopeAndVersion() {
        QTemporaryDir dir;domain::Project p;p.rootPath=dir.path();const auto s=snapshot();
        domain::AgentBaseline b{p.rootPath,s.revision,s.sourceFingerprint,"ending"};domain::AgentPlan plan;QString error;
        QVERIFY2(agent::PlanService::parse(base(),b,&plan,&error),qPrintable(error));
        QVERIFY2(agent::PlanService::validate(plan,p,s,{},&error),qPrintable(error));
        QCOMPARE(agent::PlanService::identity(plan).size(),32);
        auto stale=s;stale.revision++;QVERIFY(!agent::PlanService::validate(plan,p,stale,{},&error));
        plan.base.scope="opening";QVERIFY(!agent::PlanService::validate(plan,p,s,{},&error));
    }
    void rejectsUncontrolledAndDuplicateActions() {
        domain::AgentPlan p;QString error;auto json=base();json.insert("approved",true);
        QVERIFY(!agent::PlanService::parse(json,{},&p,&error));
        json=base();json["operations"]=QJsonArray{json["operations"].toArray().first(),json["operations"].toArray().first()};
        QVERIFY(!agent::PlanService::parse(json,{},&p,&error));
        json=base();json["kind"]="shell";QVERIFY(!agent::PlanService::parse(json,{},&p,&error));
    }
    void rejectsWrongFieldTypeAndMissingAsset() {
        QTemporaryDir dir;domain::Project project;project.rootPath=dir.path();const auto s=snapshot();domain::AgentPlan p;QString error;
        auto json=base();json["operations"]=QJsonArray{QJsonObject{{"entityId","ending"},{"fieldId","title"},{"value",42}}};
        QVERIFY(agent::PlanService::parse(json,{project.rootPath,17,s.sourceFingerprint,{}},&p,&error));
        QVERIFY(!agent::PlanService::validate(p,project,s,{},&error));
    }
    void threeScenesHaveRealBounds() {
        QJsonArray scenes;for(int i=0;i<3;i++)scenes.append(QJsonObject{{"sceneId",QString("scene-%1").arg(i+1)},{"assetId","local-image"},{"title","校园"},{"subtitle","活动"},{"color","#35bca8"},{"durationFrames",i==0?90:i==1?240:120},{"fontSize",54}});
        auto json=base();json["kind"]="create";json["projectName"]="招新故事";json["operations"]=QJsonArray{};json["scenes"]=scenes;
        domain::AgentPlan p;QString error;QVERIFY2(agent::PlanService::parse(json,{},&p,&error),qPrintable(error));
        domain::AgentAsset a;a.asset.hash="local-image";a.asset.mime="image/png";a.localPath="missing.png";
        QVERIFY(!agent::PlanService::validate(p,{}, {},{a},&error));
        auto invalid=scenes;auto scene=invalid[0].toObject();scene["durationFrames"]=1;invalid[0]=scene;json["scenes"]=invalid;
        QVERIFY(!agent::PlanService::parse(json,{},&p,&error));
    }
    void clarificationCannotMutate() {
        auto json=base();json["kind"]="clarify";json["question"]="总时长允许改为17秒吗？";json["operations"]=QJsonArray{};
        domain::AgentPlan p;QString error;QVERIFY(agent::PlanService::parse(json,{},&p,&error));
        json["operations"]=base()["operations"];QVERIFY(!agent::PlanService::parse(json,{},&p,&error));
    }
};
QTEST_GUILESS_MAIN(AgentPlanTest)
#include "AgentPlanTest.moc"
