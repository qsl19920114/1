#include "services/SampleCatalog.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
namespace qvw::services {
domain::VideoSamples SampleCatalog::discover(const QString &distribution) {
    const auto root=QFileInfo(distribution).canonicalFilePath();
    if(root.isEmpty()||!QFileInfo(root).isDir())return {};
    const QStringList known{
        "output/chat-demo.mp4",
        "examples/semantic-composition/.hypit/results/2026-09-20/bld_20260920T071529859Z_F3FD835600/files/file-0001.mp4",
        "examples/semantic-composition/.hypit/results/2026-09-23/bld_20260923T100423557Z_A07A70298A/files/file-0001.mp4"};
    domain::VideoSamples result;QStringList problems;
    for(const auto &relative:known) {
        const QFileInfo info(QDir(root).filePath(relative));const auto path=info.canonicalFilePath();
        if(!path.startsWith(root+"/")||!info.isFile()||info.isSymLink()||info.size()<=0||info.size()>64*1024*1024){problems.append(QStringLiteral("样例不存在、路径不安全或超过64MiB：%1").arg(relative));continue;}
        QFile file(path);if(!file.open(QIODevice::ReadOnly)){problems.append(QStringLiteral("无法读取样例：%1").arg(relative));continue;}
        QCryptographicHash hash(QCryptographicHash::Sha256);if(!hash.addData(&file)){problems.append(QStringLiteral("无法校验样例：%1").arg(relative));continue;}
        const auto digest=QString::fromLatin1(hash.result().toHex());
        bool duplicate=false;for(auto &sample:result)if(sample.sha256==digest){sample.sources.append(relative);duplicate=true;break;}
        if(!duplicate)result.append({QStringLiteral("对话故事 · 本地测试视频"),path,digest,{relative}});
    }
    if(result.isEmpty())result.append({QStringLiteral("本地测试视频尚未就绪"),{}, {},known,false,problems.join("\n")});
    else for(auto &sample:result)sample.error=problems.join("\n");
    return result;
}
}
