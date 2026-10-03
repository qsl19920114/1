#include "agent/StoryTemplate.h"
#include "agent/PlanService.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
namespace qvw::agent {
bool StoryTemplate::prepare(const QString &installed,const QJsonArray &scenes,const QString &destination,QString *error){
    auto fail=[&](const QString &e){if(error)*error=e;return false;};
    domain::AgentPlan plan;
    if(!PlanService::parse({{"format","qvw.agent-plan@1"},{"kind","create"},{"summary","实例化受控时间线"},{"question",""},{"projectName","故事"},{"scenes",scenes},{"operations",QJsonArray{}}},{},&plan,error))return false;
    QFile metadata(QDir(installed).filePath("template.json"));
    if(!metadata.open(QIODevice::ReadOnly)||metadata.size()>16384)return fail("可信故事模板不可读。");
    auto info=QJsonDocument::fromJson(metadata.readAll()).object();
    if(info["id"]!="story-reel"||info["version"]!="1.0.0")return fail("仅允许安装的story-reel@1模板。");
    if(QFileInfo(destination).exists()||QFileInfo(destination).isSymLink())return fail("模板暂存目录已存在，拒绝覆盖。");
    QStringList files,dirs;QDirIterator it(installed,QDir::AllEntries|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
    while(it.hasNext()){it.next();const auto f=it.fileInfo();if(f.isSymLink()||(!f.isDir()&&!f.isFile()))return fail("模板不能包含符号链接或特殊文件。");const auto relative=QDir(installed).relativeFilePath(f.filePath());if(f.isDir())dirs.append(relative);else files.append(relative);}
    if(files.size()>128||!QDir().mkpath(destination))return fail("无法创建受控模板目录。");
    for(const auto &dir:dirs)if(!QDir().mkpath(QDir(destination).filePath(dir)))return fail("模板子目录创建失败。");
    for(const auto &file:files)if(!QFile::copy(QDir(installed).filePath(file),QDir(destination).filePath(file)))return fail("可信模板复制失败。");
    int total=0;for(const auto &v:scenes)total+=v.toObject()["durationFrames"].toInt();
    QString source=QStringLiteral("<?svml using=\"@hypit/markup@1\"?>\n<svml>\n"
      "  <import as=\"time\" from=\"@hypit/timeline-author@1\"/>\n  <import as=\"spatial\" from=\"@hypit/spatial@1\"/>\n"
      "  <import as=\"fonts\" from=\"@hypit/fonts-open@1\"/>\n  <import as=\"card\" from=\"@qvw/story-reel@1\"/>\n"
      "  <import as=\"film\" from=\"@hypit/film@1\"/>\n  <import as=\"render\" from=\"@hypit/render-hyperframes@1\"/>\n"
      "  <import as=\"style\" source=\"./main.svs\"/>\n  <time:Clock id=\"clock\" frame-rate=\"30\"/>\n"
      "  <time:Timeline id=\"timeline\" clock={clock} end=\"%1f\"/>\n  <spatial:Canvas id=\"canvas\" width=\"720\" height=\"1280\"/>\n"
      "  <fonts:Stack id=\"font\" family=\"noto-sans-sc\" weight=\"600\" style=\"normal\"/>\n").arg(total);
    int start=0;
    for(const auto &v:scenes){const auto s=v.toObject();const int end=start+s["durationFrames"].toInt();
        source+=QStringLiteral("  <card:Card id=\"%1\" timeline={timeline.timeline} canvas={canvas} font={font} start=\"program.start+%2f\" end=\"program.start+%3f\" title=\"故事场景\" subtitle=\"等待应用已审阅方案\" color=\"#35bca8\" image=\"./assets/default.png\" font-size=\"54\"/>\n").arg(s["sceneId"].toString()).arg(start).arg(end);start=end;}
    source+="  <film:Film id=\"main\" canvas={canvas} timeline={timeline.timeline} appearance={style.film.main}>\n";
    for(const auto &v:scenes)source+=QStringLiteral("    <film:Track source={%1.track}/>\n").arg(v.toObject()["sceneId"].toString());
    source+="  </film:Film>\n  <render:Video id=\"final\" composition={main.composition} timeline={timeline.timeline}/>\n</svml>\n";
    QSaveFile out(QDir(destination).filePath("main.svml"));const auto bytes=source.toUtf8();
    if(!out.open(QIODevice::WriteOnly)||out.write(bytes)!=bytes.size()||!out.commit())return fail("受控时间线保存失败。");
    info["durationSeconds"]=total/30.0;QSaveFile meta(QDir(destination).filePath("template.json"));const auto metaBytes=QJsonDocument(info).toJson();
    if(!meta.open(QIODevice::WriteOnly)||meta.write(metaBytes)!=metaBytes.size()||!meta.commit())return fail("模板元数据保存失败。");
    if(error)error->clear();return true;
}
}
