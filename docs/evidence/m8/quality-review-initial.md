# 代码评审报告

- 仓库：qt-video-workbench
- 检测模式：通用检测
- 检测范围：HEAD tracked diff + explicitly scoped untracked full files, base 8fae1e8
- 生成时间：2026-10-02 23:51
- 检查文件：56
- 变更行数：5414

## 缺陷统计

- P0：0
- P1：3
- P2：0
- 合计：3

## 缺陷详情

### 1. [P1][健壮性问题] 任务记录路径允许 FIFO，恢复与事件追加会阻塞界面

- 位置：`app/agent/AgentTaskStore.cpp:14-22`
- 置信度：10/10

**问题描述**

path 仅拒绝符号链接，随后 resolvePath(...,false) 不要求终点为普通文件。打开既有工程时，若 .workbench/agent/current.json 是无 writer 的 FIFO，restore→load→QFile::open(ReadOnly) 在 UI 线程阻塞；若 events.jsonl 是无 reader 的 FIFO，generate/approve→record→append→QFile::open(WriteOnly|Append) 同样阻塞。可复现输入是对这些固定记录路径运行 mkfifo，然后打开工程或生成目标。

**修复建议**

对已存在的所有任务终点（current、events、previous、lock）要求 isFile，并要求已存在的父路径为目录；保留 symlink 拒绝与边界检查，在 QFile/QLockFile 打开前失败返回。

---

### 2. [P1][健壮性问题] Codex 子进程未继承配置后的工具 PATH，Finder 下的脚本入口无法启动

- 位置：`app/main.cpp:85-85`
- 置信度：9/10

**问题描述**

明确复现路径：以父 PATH=/usr/bin:/bin 启动应用，在 tools.codex 指定一个有效的 Codex JavaScript launcher（首行为 #!/usr/bin/env node），Node 位于 /opt/homebrew/bin 或 tools.node 指定的自定义目录。AppConfig.cpp:110 构造包含这些目录的 processEnvironment，并在118成功解析 codex；此处及230却仅 setProgram。ModelClient.cpp:224–231 创建 QProcess 后未设置环境，因此 launcher 继承原始精简 PATH，/usr/bin/env 找不到 node，真实方案生成失败。USER_GUIDE.md:16、20、76 明确支持这些 tools 配置及 Finder 启动，现有 RuntimePathsTest:22–29 只核对路径解析，不能覆盖该执行路径。

**修复建议**

为 ModelClient 增加进程环境配置接口，将 AppConfig.processEnvironment 在首次启动和 main.cpp:230 切换配置时传入，并在创建 QProcess 后调用 setProcessEnvironment；覆盖精简父 PATH 和需要 /usr/bin/env node 的 Codex 入口。

---

### 3. [P1][逻辑错误] 把 Hypit 合法文本布局的运行时 DOM 变化判为版本不匹配，永久禁用预览控制

- 位置：`app/ui/StudioTransport.cpp:49-59`
- 置信度：9/10

**问题描述**

明确复现路径：通过“更多→打开 Run”打开固定 Hypit 支持的含 text-flow、overflow=shrink 的合法工程，字体加载与布局正常完成。未执行脚本的 DOMParser 基线没有 data-hypit-text-shrink-scale；固定上游 packages/hyperframes/src/text.ts:1117/1134 无论是否实际缩小均会为已加载 DOM 新增该属性，1076–1085 还会合法调整 position/left/top/width/height；line sequences 在1162新增 data-hypit-text-physical-line。这些变化先被49/54的所有 data-* 完全相等检查拒绝，若只过滤属性仍可能被57–59的样式比较拒绝。于是 probe 永久返回 confirmed=false，poll:122 设置 m_ready=false，MainWindow.cpp:113–115 持续禁用 Qt 播放、上一/下一帧与定位，即使源指纹、srcdoc、画面和媒体均正确。合法输入可直接参照固定上游 packages/hyperframes/test/document.test.ts:117–219。

**修复建议**

根据固定 Hypit 的 text runtime 契约区分 authored 属性与运行时布局属性/样式：对 shrink-scale、physical-line 及 text-flow shrink 的 position/left/top/width/height 做精确的运行时处理，同时保留源码、静态文本/素材/其他属性的版本校验；不要笼统忽略所有 data 属性或样式。

---
