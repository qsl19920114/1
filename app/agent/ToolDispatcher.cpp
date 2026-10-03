#include "agent/ToolDispatcher.h"
namespace qvw::agent {
const domain::InspectorField *ToolDispatcher::field(const domain::Snapshot &s,const QString &entity,const QString &id){
    const domain::InspectorField *found=nullptr;
    for(const auto &t:s.tracks)for(const auto &c:t.clips)if(c.id==entity)for(const auto &f:c.inspector)if(f.id==id){if(found)return nullptr;found=&f;}return found;
}
QJsonArray ToolDispatcher::creationEdits(const domain::AgentPlan &p,const domain::Snapshot &snapshot,const QMap<QString,domain::Asset> &assets,QString *error){
    QJsonArray result;
    for(const auto &v:p.content["scenes"].toArray()){
        const auto scene=v.toObject();const auto id=scene["sceneId"].toString();const domain::Clip *clip=nullptr;
        for(const auto &t:snapshot.tracks)for(const auto &c:t.clips)if(c.authoredId==id){if(clip){if(error)*error="场景实体重复。";return {};}clip=&c;}
        if(!clip){if(error)*error="编译快照缺少计划场景："+id;return {};}
        const auto asset=assets.value(scene["assetId"].toString());if(asset.path.isEmpty()){if(error)*error="未成功导入场景素材。";return {};}
        const QMap<QString,QVariant> changes{{"title",scene["title"].toString()},{"subtitle",scene["subtitle"].toString()},{"color",scene["color"].toString()},{"font-size",scene["fontSize"].toInt()},{"image","./"+asset.path}};
        for(auto it=changes.cbegin();it!=changes.cend();++it){const domain::InspectorField *f=nullptr;for(const auto &candidate:clip->inspector)if(candidate.binding==it.key()&&candidate.isEditable()){if(f){if(error)*error="场景字段重复。";return {};}f=&candidate;}
            if(!f){if(error)*error="場景未开放真实可写字段："+it.key();return {};}result.append(QJsonObject{{"entityId",clip->id},{"fieldId",f->id},{"value",QJsonValue::fromVariant(it.value())}});
        }
    }
    if(error)error->clear();return result;
}
}
