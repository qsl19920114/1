# 原创、复用及第三方材料

## 原创范围

本仓库实现Qt桌面工程/素材管理、原创标题卡及其Author/Companion包、原生属性与历史、版本与Source保护、冻结导出任务编排、媒体验收、提案约束与确认，以及发布验证。源码不包含上游Hypit实现。

## 复用与依赖

| 组件 | 作用及分发范围 | 来源/版本 |
|---|---|---|
| Qt / WebEngine | Widgets、HTTP、进程及网页预览；实际动态运行库随.app部署 | Qt6.11.2，官方[部署说明](https://doc.qt.io/qt-6.11/macos-deployment.html) |
| Hypit | 编译、Studio、CLI、Worker与Provider；外部Distribution | 0.2.10，[固定源码](https://github.com/hypit-ai/hypit/tree/1af179d3f58284c2d6d3c1f63052172a4fe1b5a6) |
| Node、FFmpeg/ffprobe、渲染浏览器 | 外部执行依赖，不加入.app/ZIP | 本机版本和路径见实际发布报告 |
| Qt所带第三方库 / Homebrew依赖 | 随实际动态库部署；许可证与SBOM收集 | 实际Mach-O清单及Resources/licenses/sources.json |

Hypit LICENSE是带附加条件的modified Apache文本，不能简化为无条件Apache-2.0。包内材料保留固定版本完整原文；实际嵌入Studio的上游名称/版权标识保留。本项目名称不暗示官方合作。

Qt公开头文件给出多许可选项，具体模块与第三方组件以原始许可、SBOM及归属文本为准；本说明不替用户选择商业/开源许可或作授权审查结论。应用自己的原创代码也不因为链接Qt而被自动授予某个新的许可证。

## 随包材料与来源

收集脚本保存Qt6.11.2官方[WebEngine licensing索引](https://doc.qt.io/qt-6.11/qtwebengine-licensing.html)及126个对应组件归属HTML，完整页脚保留，另附GFDL1.3全文。QtChromium顶层BSD单独保留，不将它当成全部第三方notices。FreeType2.14.3缺失的许可及模块声明从官方镜像固定tag补取。

GNU通用文本、各候选动态库安装目录的LICENSE/COPYING/NOTICE等、Qt模块SBOM、固定HypitLICENSE一并收集；sources.json记录出处、字节数和SHA256。最终实际部署链接清单独立记录。Qt官方页的来源/标题及完整文本保留，方便核对。

这些是归属、许可文本与版本材料，不等于相应源码、构建材料、公开再分发条件或商业授权已全部审核。当前包为本地课程演示交付；未公证，未完成公开发行授权审查。修改或重新分发时应按照实际所用组件的条款补齐自己的分发义务。
