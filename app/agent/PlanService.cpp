#include "agent/PlanService.h"
#include "services/ProposalService.h"
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
namespace qvw::agent {
namespace {
bool fail(QString *error,const QString &text){if(error)*error=text;return false;}
bool keys(const QJsonObject &o,const QSet<QString> &expected){QSet<QString> actual;for(auto i=o.begin();i!=o.end();++i)actual.insert(i.key());return actual==expected;}
bool text(const QJsonValue &v,int max,bool empty=true){return v.isString()&&v.toString().toUtf8().size()<=max&&!v.toString().contains(QChar::Null)&&(empty||!v.toString().trimmed().isEmpty());}
bool integer(const QJsonValue &v,int min,int max){return v.isDouble()&&std::isfinite(v.toDouble())&&std::floor(v.toDouble())==v.toDouble()&&v.toDouble()>=min&&v.toDouble()<=max;}
QJsonObject objectSchema(QJsonObject properties){QJsonArray required;for(auto i=properties.begin();i!=properties.end();++i)required.append(i.key());return {{"type","object"},{"properties",properties},{"required",required},{"additionalProperties",false}};}
}
QJsonObject PlanService::schema(){
    const QJsonObject string{{"type","string"}},number{{"type","number"}};
    const auto scene=objectSchema({{"sceneId",string},{"assetId",string},{"title",string},{"subtitle",string},{"color",string},{"durationFrames",QJsonObject{{"type","integer"}}},{"fontSize",number}});
    const QJsonObject primitive{{"anyOf",QJsonArray{string,number,QJsonObject{{"type","boolean"}}}}};
    const auto operation=objectSchema({{"entityId",string},{"fieldId",string},{"value",primitive}});
    return objectSchema({{"format",QJsonObject{{"type","string"},{"enum",QJsonArray{"qvw.agent-plan@1"}}}},{"kind",QJsonObject{{"type","string"},{"enum",QJsonArray{"create","edit","clarify"}}}},{"summary",string},{"question",string},{"projectName",string},{"scenes",QJsonObject{{"type","array"},{"items",scene}}},{"operations",QJsonObject{{"type","array"},{"items",operation}}}});
}
bool PlanService::parse(const QJsonObject &o,const domain::AgentBaseline &base,domain::AgentPlan *out,QString *error){
    if(!out||QJsonDocument(o).toJson(QJsonDocument::Compact).size()>65536)return fail(error,"方案为空或超过64KiB。");
    if(!keys(o,{"format","kind","summary","question","projectName","scenes","operations"})||o["format"]!="qvw.agent-plan@1")return fail(error,"方案格式或键不受支持。");
    const auto kind=o["kind"].toString();
    if(!QStringList{"create","edit","clarify"}.contains(kind)||!text(o["summary"],4096,false)||!text(o["question"],4096)||!text(o["projectName"],160)||!o["scenes"].isArray()||!o["operations"].isArray())return fail(error,"方案类型或文字无效。");
    const auto scenes=o["scenes"].toArray(),operations=o["operations"].toArray();
    if(kind=="clarify"){
        if(!text(o["question"],4096,false)||!scenes.isEmpty()||!operations.isEmpty())return fail(error,"澄清不能同时包含执行操作。");
    }else if(!o["question"].toString().isEmpty())return fail(error,"需要澄清的问题必须先解决，不能同时执行。");
    if(kind=="create"){
        if(!text(o["projectName"],160,false)||scenes.size()!=3||!operations.isEmpty())return fail(error,"创建必须包含三个场景，且不带任意编辑操作。");
        QSet<QString> ids;int total=0;
        static const QRegularExpression color("^#[0-9a-fA-F]{6}$");
        for(const auto &v:scenes){
            if(!v.isObject())return fail(error,"场景必须为对象。");const auto s=v.toObject();const auto id=s["sceneId"].toString();
            if(!keys(s,{"sceneId","assetId","title","subtitle","color","durationFrames","fontSize"})||!QStringList{"scene-1","scene-2","scene-3"}.contains(id)||ids.contains(id))return fail(error,"场景ID必须唯一，且只使用scene-1/2/3。");
            if(!text(s["assetId"],256,false)||!text(s["title"],240,false)||s["title"].toString().size()>60||!text(s["subtitle"],600)||s["subtitle"].toString().size()>160||!color.match(s["color"].toString()).hasMatch()||!integer(s["durationFrames"],30,900)||!integer(s["fontSize"],32,76))return fail(error,"场景文字、颜色、字号或时长超出模板能力。");
            ids.insert(id);total+=s["durationFrames"].toInt();
        }
        if(total<90||total>1800)return fail(error,"总时长需为3–60秒。");
    }else if(!scenes.isEmpty())return fail(error,"编辑/澄清不能创建场景。");
    if(kind=="edit"){
        if(operations.isEmpty()||operations.size()>24)return fail(error,"编辑需包含1–24项操作。");QSet<QString> fields;
        for(const auto &v:operations){
            if(!v.isObject())return fail(error,"操作必须为对象。");const auto op=v.toObject();const auto value=op["value"];
            const auto id=op["entityId"].toString()+QChar(0x1f)+op["fieldId"].toString();
            if(!keys(op,{"entityId","fieldId","value"})||!text(op["entityId"],256,false)||!text(op["fieldId"],256,false)||fields.contains(id)||!(value.isBool()||(value.isDouble()&&std::isfinite(value.toDouble()))||text(value,8192)))return fail(error,"操作字段、类型或重复项无效。");
            fields.insert(id);
        }
    }else if(!operations.isEmpty())return fail(error,"非编辑方案不能包含参数操作。");
    *out={o,base};if(error)error->clear();return true;
}
bool PlanService::validate(const domain::AgentPlan &p,const domain::Project &project,const domain::Snapshot &s,const domain::AgentAssets &assets,QString *error){
    domain::AgentPlan checked;if(!parse(p.content,p.base,&checked,error))return false;
    if(p.base.root!=project.rootPath||(!p.base.root.isEmpty()&&(p.base.revision!=s.revision||p.base.fingerprint!=s.sourceFingerprint)))return fail(error,"工程或版本已改变，请根据当前状态重新生成或审阅方案。");
    if(p.kind()=="create"){
        for(const auto &v:p.content["scenes"].toArray()){
            const auto id=v.toObject()["assetId"].toString();const domain::AgentAsset *found=nullptr;
            for(const auto &a:assets)if(a.asset.hash==id){if(found)return fail(error,"素材ID重复。");found=&a;}
            if(!found||(found->asset.mime!="image/png"&&found->asset.mime!="image/jpeg")||!QFileInfo(found->localPath).isFile()||QFileInfo(found->localPath).isSymLink())return fail(error,"创建仅可绑定已选择、可读的本地PNG/JPEG图片。");
        }
    }else if(p.kind()=="edit"){
        for(const auto &v:p.content["operations"].toArray()){
            const auto op=v.toObject();domain::EditProposal edit;edit.revision=s.revision;edit.sourceFingerprint=s.sourceFingerprint;edit.entityId=op["entityId"].toString();edit.fieldId=op["fieldId"].toString();edit.value=op["value"].toVariant();
            if(!p.base.scope.isEmpty()&&edit.entityId!=p.base.scope)return fail(error,"操作超出选中的组件范围。");
            if(!services::ProposalService::validate(edit,s,project,error))return false;
        }
    }
    if(error)error->clear();return true;
}
QByteArray PlanService::identity(const domain::AgentPlan &p){
    QJsonObject o{{"plan",p.content},{"project",p.base.root},{"revision",p.base.revision},{"fingerprint",QString::fromLatin1(p.base.fingerprint.toHex())},{"scope",p.base.scope}};
    return QCryptographicHash::hash(QJsonDocument(o).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256);
}
}
