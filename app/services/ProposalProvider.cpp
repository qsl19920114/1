#include "services/ProposalProvider.h"
#include <QRegularExpression>
namespace qvw::services {
namespace {
bool fail(QString *error,const QString &text){if(error)*error=text;return false;}
}
bool DemoProposalProvider::generate(const QString &input,const domain::Snapshot &snapshot,const domain::Project &project,domain::EditProposal *proposal,QString *error) const {
    if(!proposal)return fail(error,QStringLiteral("提案输出为空。"));
    if(project.templateId!="title-card")return fail(error,QStringLiteral("本地模拟仅支持声明了 title/color/image 的标题卡模板。"));
    if(input.toUtf8().size()>8192)return fail(error,QStringLiteral("模拟指令超过 8192 字节上限。"));
    const auto text=input.trimmed();QString binding;QVariant value;
    if(text.startsWith(QStringLiteral("标题改为"))) {binding="title";value=text.mid(4);if(value.toString().trimmed().isEmpty())return fail(error,QStringLiteral("标题不能为空。"));}
    else if(text.startsWith(QStringLiteral("主题色改为"))) {
        binding="color";auto color=text.mid(5);if(color==QStringLiteral("橙色"))color="#e47735";
        static const QRegularExpression pattern(QStringLiteral("^#[0-9a-fA-F]{6}$"));
        if(!pattern.match(color).hasMatch())return fail(error,QStringLiteral("主题色使用 #RRGGBB 或橙色。"));value=color;
    } else {
        static const QRegularExpression image(QStringLiteral("^图片使用第([1-9][0-9]*)张$"));const auto match=image.match(text);bool ok=false;
        const auto index=match.hasMatch()?match.captured(1).toInt(&ok):0;
        if(!ok||index<1||index>project.assets.size())return fail(error,QStringLiteral("模拟支持：标题改为…、主题色改为#RRGGBB、图片使用第N张（工程素材编号）。"));
        binding="image";value="./"+project.assets[index-1].path;
    }
    domain::EditProposal candidate;candidate.revision=snapshot.revision;candidate.sourceFingerprint=snapshot.sourceFingerprint;
    candidate.value=value;candidate.origin=domain::ProposalOrigin::Demo;int count=0;
    for(const auto &track:snapshot.tracks)for(const auto &clip:track.clips)for(const auto &field:clip.inspector)
        if(field.binding==binding&&field.isEditable()) {++count;candidate.entityId=clip.id;candidate.fieldId=field.id;}
    if(count!=1)return fail(error,QStringLiteral("当前模板没有唯一且可写的 %1 binding；模拟不会猜测属性。").arg(binding));
    if(!ProposalService::validate(candidate,snapshot,project,error))return false;
    *proposal=std::move(candidate);if(error)error->clear();return true;
}
}
