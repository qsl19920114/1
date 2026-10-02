# 代码评审报告

- 仓库：qt-video-workbench
- 检测模式：通用检测（M6首轮只读质量审查）
- 检测范围：991e620 → 当前工作区，包含未跟踪源码、测试、配置、发布脚本和指南
- 生成时间：2026-10-02 12:18
- 检查文件：42
- 变更行数：1399

## 缺陷统计

- P0：0
- P1：1
- P2：2
- 合计：3

## 缺陷详情

### 1. [P1][逻辑错误] 自动使用 Node 会破坏合法的 shell 启动器配置

- 位置：`app/infrastructure/HypitProbe.cpp:59-61`
- 置信度：9/10

**问题描述**

当用户沿用 hypit.launcher="hypit" 且 PATH 中存在 Node 时，loadAppConfig 新增的解析逻辑会自动填充 nodePath，即使用户未声明 tools.node。这里随后执行 node <distribution>/hypit version --json。固定版本 Hypit 自带的根目录 hypit 是 POSIX shell 脚本（上游 hypit:1–11，内部再 exec node bin/hypit.mjs），不能被 Node 当作 JavaScript 解析，因此原先直接执行该启动器可用的配置现在必然版本自检失败，无法打开工程。docs/API_CONTRACT.md:13–20 也记录了 ./hypit 的真实可用启动契约。StudioProcess 和 ExportController 使用相同的 nodePath 判断，具有同一根因。

**修复建议**

保留可执行 shell 启动器的直接执行方式；只有确认 launcher 是 Node 脚本时才将显式或解析出的 Node 作为 program。将工具解析结果与启动器执行模式分开，并增加加载配置后使用官方 shell launcher 的回归覆盖。

---

### 2. [P2][并发问题] 多实例日志轮转会永久停止另一实例的日志

- 位置：`app/infrastructure/LogWriter.cpp:45-47`
- 置信度：9/10

**问题描述**

默认日志在 AppConfig.cpp:38–40/126 和 main.cpp:68–72 固定为相同 AppDataLocation/workbench.jsonl，应用没有单实例约束或跨进程文件锁。多个应用实例共用该文件时，实例 A 在 rotate 的 QFile::rename(m_filePath, first) 后、append 重新创建当前日志文件前存在缺文件窗口。实例 B 此时进入这里会因 !current.isFile() 调用 fail，将自己的 m_ready 永久设为 false；此后 append 第 68 行持续提前返回，即使 A 已重建文件，B 的外部命令和错误日志也再不记录。这是新增轮转引入的合法并发时序。

**修复建议**

使用 QLockFile 等跨进程锁覆盖检查大小、轮转和追加的完整操作，或为每个实例使用独立日志文件；避免将其他实例正常轮转造成的短暂缺文件直接转为永久停写状态。

---

### 3. [P2][业务语义问题] 启动验证未完成时关闭窗口仍返回 PASS

- 位置：`app/main.cpp:247-250`
- 置信度：9/10

**问题描述**

以 --verify-startup 启动后，在自检或预览验证完成前关闭唯一窗口。默认 Qt lastWindowClosed 会 quit，app.exec() 返回 0；aboutToQuit 仅停止计时器并清理 Studio，cleanupStopped 可为 true。finalCode 与报告 verdict 只根据退出码，因此 Snapshot、pageLoaded、compiledCompositionReady 或 imagesReady 为 false 时仍写出 PASS 并退出 0。搬移脚本另有布尔值断言能拦截这一情况，但 CLI 自身的成功结果违反真实启动验证语义。

**修复建议**

设置独立的 verificationSucceeded 标志，仅在真实 Snapshot、页面、composition、非空图片集合均通过的成功分支设置；verify 模式退出 0 时若没有此标志，应改为非零并写出提前结束的 FAIL 原因。

---

## 审查覆盖与后续

42个文件，约1399行本地变更，包含全部未跟踪C++源码、单测、WalkthroughDemo、Python发布脚本、release CMake与bundle配置，以及新增使用/第三方/演示/测试指南。按资源与子进程、发布流程、缓存与入口三个功能组审查，覆盖逻辑、业务语义、路径与文件安全、并发、健壮性和实际性能风险。仅报告有具体触发路径的确定P0–P2。

审查者只读源码、直接调用契约与既有证据；未重跑构建、真实Runtime、打包或耗时测试，未改产品代码，未发外部评论，未打开报告。发布组没有确定缺陷，Python3.9 API兼容。真实包的依赖闭包、RPATH/helper修复和明示验收边界不重复列为缺陷。

主代理已接受三项缺陷，须修复后复审；本报告不表示G6通过。当前首轮结论：**REQUIRES_FIXES（P1×1，P2×2，P0×0）**。

[HTML报告](quality-review.html) ｜ [结构化缺陷](quality-review.json)
