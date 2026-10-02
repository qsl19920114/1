#include "services/ProposalService.h"
#include "services/ProjectStore.h"
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
#include <limits>
namespace qvw::services {
namespace {
bool fail(QString *error,const QString &text) {if(error)*error=text;return false;}
bool boundedString(const QJsonValue &value,int limit,bool nonempty=true) {
    return value.isString()&&value.toString().toUtf8().size()<=limit
        &&(!nonempty||!value.toString().trimmed().isEmpty())&&!value.toString().contains(QChar::Null);
}
bool simpleValue(const QJsonValue &value) {
    return value.isBool()||(value.isDouble()&&std::isfinite(value.toDouble()))||boundedString(value,8192,false);
}
const domain::InspectorField *findField(const domain::Snapshot &snapshot,const domain::EditProposal &proposal) {
    const domain::InspectorField *found=nullptr;
    for(const auto &track:snapshot.tracks)for(const auto &clip:track.clips)if(clip.id==proposal.entityId)
        for(const auto &field:clip.inspector)if(field.id==proposal.fieldId) {
            if(found)return nullptr;found=&field;
        }
    return found;
}
bool typedValue(const domain::InspectorField &field,const QVariant &variant) {
    const auto value=QJsonValue::fromVariant(variant),current=QJsonValue::fromVariant(field.rawValue);
    if(!simpleValue(value))return false;
    switch(field.control) {
    case domain::ControlKind::Number: {
        bool ok=false;const auto number=field.rawValue.toDouble(&ok);
        return (current.isDouble()||current.isString())&&ok&&std::isfinite(number)&&value.isDouble();
    }
    case domain::ControlKind::Boolean:
        return value.isBool()&&(current.isBool()||(current.isString()&&(current.toString()=="true"||current.toString()=="false")));
    case domain::ControlKind::Text:return current.isString()&&value.isString();
    case domain::ControlKind::Color: {
        static const QRegularExpression color(QStringLiteral("^#[0-9a-fA-F]{6}$"));
        return current.isString()&&value.isString()&&color.match(value.toString()).hasMatch();
    }
    case domain::ControlKind::Select:
        if(value.type()!=current.type())return false;
        for(const auto &option:field.options)if(QJsonValue::fromVariant(option.value)==value)return true;
        return false;
    case domain::ControlKind::Unsupported:return false;
    }
    return false;
}
QString display(const QVariant &value) {
    if(value.typeId()==QMetaType::Bool)return value.toBool()?QStringLiteral("true"):QStringLiteral("false");
    return value.toString();
}
}
bool ProposalService::parseJson(const QByteArray &bytes,domain::ProposalOrigin origin,domain::EditProposal *proposal,QString *error) {
    if(!proposal)return fail(error,QStringLiteral("提案输出为空。"));
    if(bytes.isEmpty()||bytes.size()>MaxJsonBytes)return fail(error,QStringLiteral("提案 JSON 为空或超过 64KiB 上限。"));
    QJsonParseError parseError;const auto document=QJsonDocument::fromJson(bytes,&parseError);
    if(parseError.error!=QJsonParseError::NoError||!document.isObject())return fail(error,QStringLiteral("提案必须是一个合法 JSON 对象。"));
    const auto object=document.object();
    static const QSet<QString> allowed{"format","revision","sourceFingerprint","entityId","fieldId","value","origin"};
    QSet<QString> keys;for(auto it=object.constBegin();it!=object.constEnd();++it)keys.insert(it.key());
    if(keys!=allowed)return fail(error,QStringLiteral("提案必须且只能包含 format、revision、sourceFingerprint、entityId、fieldId、value、origin。"));
    if(!object["format"].isString()||object["format"].toString()!="qvw.edit-proposal@1")return fail(error,QStringLiteral("提案 format 不受支持。"));
    const auto revision=object["revision"];
    if(!revision.isDouble()||!std::isfinite(revision.toDouble())||revision.toDouble()<0
        ||std::floor(revision.toDouble())!=revision.toDouble()||revision.toDouble()>std::numeric_limits<int>::max())
        return fail(error,QStringLiteral("提案 revision 必须是有效的非负整数。"));
    static const QRegularExpression fingerprint(QStringLiteral("^[0-9a-f]{64}$"));
    if(!object["sourceFingerprint"].isString()||!fingerprint.match(object["sourceFingerprint"].toString()).hasMatch())
        return fail(error,QStringLiteral("提案 sourceFingerprint 必须是 64 位小写 SHA256 十六进制。"));
    if(!boundedString(object["entityId"],256)||!boundedString(object["fieldId"],256)||!boundedString(object["origin"],128))
        return fail(error,QStringLiteral("提案实体、属性或来源的类型、长度无效。"));
    if(!simpleValue(object["value"]))return fail(error,QStringLiteral("提案 value 只能是布尔、有限数值或最多 8192 字节的文本。"));
    domain::EditProposal parsed;parsed.revision=revision.toInt();
    parsed.sourceFingerprint=QByteArray::fromHex(object["sourceFingerprint"].toString().toLatin1());
    parsed.entityId=object["entityId"].toString();parsed.fieldId=object["fieldId"].toString();parsed.value=object["value"].toVariant();
    // The importer determines provenance; JSON self-description is never trusted.
    parsed.origin=origin;*proposal=std::move(parsed);if(error)error->clear();return true;
}
bool ProposalService::validate(const domain::EditProposal &proposal,const domain::Snapshot &snapshot,const domain::Project &project,QString *error) {
    const QFileInfo root(project.rootPath);
    if(!QDir::isAbsolutePath(project.rootPath)||!root.isDir()||root.canonicalFilePath().isEmpty())return fail(error,QStringLiteral("请先打开有效工程。"));
    if(!snapshot.isLoaded()||snapshot.sourceFiles.isEmpty()||snapshot.sourceFingerprint.size()!=32)
        return fail(error,QStringLiteral("当前会话快照尚未就绪。"));
    if(proposal.revision!=snapshot.revision||proposal.sourceFingerprint!=snapshot.sourceFingerprint)
        return fail(error,QStringLiteral("提案的 revision 或源码指纹已过期，请重新生成。"));
    if(!boundedString(proposal.entityId,256)||!boundedString(proposal.fieldId,256))return fail(error,QStringLiteral("提案实体或属性无效。"));
    const auto *field=findField(snapshot,proposal);
    if(!field||!field->isEditable()||!typedValue(*field,proposal.value))
        return fail(error,QStringLiteral("提案属性不存在、不可写，或 value 与当前属性类型不匹配。"));
    if(field->binding=="image") {
        auto path=proposal.value.toString();if(path.startsWith("./"))path.remove(0,2);
        const domain::Asset *registered=nullptr;
        for(const auto &asset:project.assets)if(asset.path==path){if(registered)return fail(error,QStringLiteral("工程素材路径登记重复。"));registered=&asset;}
        if(!registered||(registered->mime!="image/png"&&registered->mime!="image/jpeg"))
            return fail(error,QStringLiteral("图片提案只能使用当前工程登记的 PNG/JPEG 素材。"));
        QString absolute;if(!ProjectStore::resolvePath(project,path,&absolute,error))return false;
        const QFileInfo file(absolute);
        if(!file.isFile()||!file.isReadable()||file.isSymLink())return fail(error,QStringLiteral("登记素材不可读或已被替换为符号链接。"));
    }
    if(error)error->clear();return true;
}
QString ProposalService::summary(const domain::EditProposal &proposal,const domain::Snapshot &snapshot) {
    const auto *field=findField(snapshot,proposal);if(!field)return {};
    return QStringLiteral("属性：%1\n当前值：%2\n拟修改：%3\n来源：%4")
        .arg(field->label.isEmpty()?field->id:field->label,display(field->rawValue),display(proposal.value),domain::proposalOriginLabel(proposal.origin));
}
}
