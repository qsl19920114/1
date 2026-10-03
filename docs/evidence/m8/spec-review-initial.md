# M8–M10 Agent 创作闭环规格审查（初审）

结论：**NEEDS FIXES / NOT FULLY VERIFIED**。发现 4 项 P1、1 项 P2；真实完整闭环当前不能标 PASS。本报告记录初审版本，主任务已经开始修复其中的问题，修复状态需重新构建后复验。没有修改或 stage 源代码。

## 范围与方法

按 `AGENTS.md`、已批准的 `docs/plans/2026-10-02-agent-creation.md` 及父任务提供的用户要求核对。范围为 ModelClient 以外的 `app/agent/`、`domain/AgentPlan.h`、Snapshot.authoredId/mapper、story-reel、AgentPanel/PlanReviewPanel、MainWindow/main 及相关测试，共 40 个文本文件约 2695 行。读取直接调用的 Document/Editor/Export、ProposalService 和 ProjectStore 契约；不重复 ModelClient 已独立通过的审查。

采用 bits-code-guard 的分级、范围及误报规则。父任务要求不再 spawn，分三组在本 agent 内串行审查并核对跨组调用。新增未跟踪文件显式纳入 full_file 范围，未依赖 `git diff HEAD` 的遗漏结果。C++ 使用通用维度，模板 JS 使用专项规则。行为缺陷用独立临时 C++ 探针链接当时已有构建的真实控制器/服务；探针仅在 QTemporaryDir 内创建工程，不调用模型、Studio 或 Build，也没有假称真实集成通过。

探针源码：[restore_probe.cpp](/tmp/qt-video-workbench_m8_spec_review/restore_probe.cpp)。原始结果：[spec-review-probe-initial.log](spec-review-probe-initial.log)。源码清单/摘要：[spec-review-sources-initial.json](spec-review-sources-initial.json)，记录报告组装时的工作区文件；父任务并行修复，初始行为以探针构建及下方初始代码片段为准。

## 已确认行为缺陷

### 1. [P1] 任务事件可以通过工程内符号链接改写源码

位置：`app/agent/AgentTaskStore.cpp:10`（path）；实际写入路径在 append。置信度 10/10。

```cpp
if(!services::ProjectStore::resolvePath(p,relative,out,error,false))return false;
if(create&&!QDir().mkpath(QFileInfo(*out).absolutePath()))return fail(...);
return services::ProjectStore::resolvePath(p,relative,out,error,false);
```

该 resolvePath 只拒绝解析后位于工程外的路径；工程内部链接仍被允许。临时工程将 `.workbench/agent/events.jsonl` 链接至同工程 `main.svml`，追加事件返回成功并改变真实源码：`internal symlink linked=1 eventAppend=1 authoredSourceMutated=1`。这不满足 T041“禁止符号链接”，且会损坏工程输入。

修复：所有任务目录段和 current/events/previous/lock 文件都显式拒绝符号链接，并对内部、外部链接分别验证源码与目标文件不变。

### 2. [P1] 指纹不匹配的任务在第二次恢复时可重新批准

位置：`app/agent/AgentController.cpp:132`（restore）与 persist。置信度 10/10。

```cpp
const auto saved=readBase(state["executionBase"].toObject());
// 初始实现没有 m_executionBase=saved。
if(saved.fingerprint!=m_editor.snapshot().sourceFingerprint){
    m_status.canApprove=false;publish("stale",...);return;
}
// persist 对空 m_executionBase 使用 currentBase。
```

在新 Controller 中恢复已被人工改动的工程时，首次拒绝是正确的，但 publish 会保存任务，把保存的旧基线替换成当前基线。探针结果：首次 `stale canApprove=0`；随后 `saved baseline changed to current=1`；第二次 `review canApprove=1`。旧剩余操作由此失去“来源版本已过期”的证据，违反匹配事实后才能恢复剩余动作的规格。

修复：保留 saved 基线；stale 发布不能把旧方案迁移成当前方案。连续两次恢复和重启后的重复恢复都应保持过期状态，直至用户根据真实现状产生并确认新方案。

### 3. [P1] 恢复后的局部任务失去保存的作用范围

位置：`app/agent/AgentController.cpp:132`（restore）。置信度 10/10。

初始恢复读回 goal/operations，却没有恢复 state.scope 或 executionBase.scope，随后使用 currentBase 生成新审批基线。探针在合法保存的 `ending` 范围任务上得到 `matching restore phase=review statusScope= planScope=`。Qt 会显示当前工程范围，修订请求也失去原有的实体限制。

修复：明确恢复并校验保存的 scope，使 status、方案基线和修订上下文一致；重新确认仍保持局部范围，不采用新进程默认的空 scope。

恢复生命周期还存在同源问题：新 generate 没有把执行基线重置到此次请求基线，projectLoaded/documentClosed 也未清旧执行基线。此前任务后手动修改再生成，会把旧指纹存入新待审阅任务；切到另一工程后生成，会命中 persist 的旧 root 保护并不保存新任务 JSON。已独立通知主任务修复，未额外增加缺陷统计。

### 4. [P1] 编译快照被标成可视预览确认版本

位置：`app/ui/AgentPanel.cpp:107`（updateVersions）。置信度 9/10。

```cpp
const auto preview = m_snapshot.isLoaded()
    ? QStringLiteral("预览确认 v%1").arg(m_snapshot.revision)
    : QStringLiteral("预览未确认");
```

该标签由 Editor 的编译 Snapshot 更新，不接收浏览器/Studio 的实际预览版本。MainWindow 的 loadFinished 和 StudioTransport 的真实 composition 就绪观察都没有更新此标签。因此页面尚未加载、渲染失败或仍显示旧内容时，也可显示最新“预览确认”。已有 UI 测试只验证了这段派生文本，不能证明实际预览版本。

修复：现有事实准确标为“编译快照”；实际可视预览的 revision/fingerprint 需另行确认，未取得时显示待确认，不能仅从编译成功推导。

### 5. [P2] 损坏的恢复记录没有错误提示

位置：`app/agent/AgentController.cpp:132`（restore）。置信度 10/10。

```cpp
if(!AgentTaskStore::load(m_document.project(),&state,&error))return;
```

load 已区分非法 JSON、超过上限和不支持版本，但 Controller 丢弃所有错误。将 current.json 写成非法 JSON 的探针得到 `corrupt restore phase=idle failureSignals=0`。用户点击恢复没有得到规格要求的明确原因。

修复：区分没有任务与损坏/不支持记录；后者展示失败原因并确保没有可执行批准。

## 静态核对满足的设计

| 要求 | 当前核对结果 | 证据边界 |
|---|---|---|
| 三类类型化方案、额外键/混合操作/类型/重复项拒绝 | parse 有精确键集、简单类型、3 场景/24 操作上限；validate 复用 ProposalService | Plan 单测 7/7；未替代真实请求和执行 |
| 工程事实、Qt entity、真实可写字段与登记素材 | ContextBuilder 从 Project/Snapshot/素材元数据生成，声明 pixels=false；有 authoredId，写操作继续用完整 compiled id | 字段能力源头为真实 Snapshot；本轮 authoredId 修复仍待完整实测 |
| 本地授权，方案编辑后重新校验 | ApprovalManager 将 plan+root+revision+fingerprint+scope 做 hash；main 对 updatePlan 后的完整 content 相等性及 canApprove 检查再批准 | 主路径静态成立；无模型 self approve API |
| 可信创建和文本参数编辑 | StoryTemplate 只生成受控 ID/时长/顺序；模型文案通过 Editor 参数进入，创建经 Document/AssetService | 顺序/总时长模板单测存在；实际所有创建写入尚未通过 |
| 顺序执行、成功前缀、停止、重试上限 | Controller 单写入推进，每项 observation 后记录；最多 24 步，失败步最多重试 2 次；stop 不回滚已确认写入 | 最新真实日志只确认 1 项成功后失败保留；负向完整覆盖仍缺 |
| 有界修订与新确认 | repair 以当前 Project/Snapshot 和一次失败结果请求新方案；每次按钮发一个 ModelClient 请求，仍回 review 重新批准 | 足够符合本轮规划/失败修订边界；不能宣称已实测恢复所有异常 |
| 共用手动版本保护和 history | 执行/undo 调用同一个 EditorController，preflight、ack 回读、history 复用 | Editor 既有验证可复用；Agent 多步场景还需复验 |
| 导出当前工程事实 | Agent.startExport 使用当前 Editor Snapshot，委托既有 ExportController；导出仍由用户路径选择触发；Build ID/产物追加记录 | frozen 输入、Build/Result/get/媒体解码语义已有 ExportController；本轮新闭环最终导出仍待通过 |
| 任务日志与有界落盘 | QSaveFile、锁、256KiB current 与1MiB JSONL轮转；批准不在 restore 中读回 | 路径与恢复问题见上；文件格式通过不等于恢复正确 |
| 无假百分比和观察/取消区别 | Agent 只显示完成项数；取消Build/停止观察继续走两个既有 ExportController 动作 | UI静态及已有窗口测试支持 |

## 尚缺验证，不能当成已实现缺陷

1. 最新 `agent-e2e-run.log` 仍失败：真实创建到第 1/15 项成功，第 2 项因 `The Source changed outside Studio.` 拒绝。此前模板元素 order 与 compiled ID 问题已有真实失败日志、父任务已修；完整 15 次写入、结尾修改、共享 history、重开、真实最新导出及全片解码必须在最终代码重新跑通，不能由 7/7、4/4 或 16/16 模拟/组件测试替代。
2. AgentController 单测当前只覆盖“模型提案不修改工程直到批准”。部分失败后重试不重做前缀、停止后续、人工改动使旧批准失效、损坏/过期/受限范围恢复与重开后不能继承批准，缺少控制器级负向回归证据。需要先记录 RED，再以最终实现验证；这与上方已复现行为缺陷分开。
3. 现有 AgentE2ETest 通过测试驱动调用 updatePlan/approve，GUI 卡片由独立 UI 测试验证。不得把该测试描述为用户已在真实 GUI 上审阅批准。main 中一次完整方案确认的接线静态可核对，最终 GUI 联动仍需相应证据。
4. 工程/编译快照事实可以显示；可视预览最新版本的证据尚不存在。简单改标签可以关闭误导性陈述，但若交付要求“可视预览确认版本”，仍需固定 Hypit 契约和真实确认，不能宣称这一项已通过。
5. 用户要求的视频既有 template 参数编辑有服务路径支持，但本轮真实模型对既有 video template 字段的集成证据尚未见。配音、字幕对齐、任意剪辑和像素理解属于已批准后续扩展，不计为本轮缺口。

## 审查产物与状态

主任务已收到五项确定问题和恢复生命周期附注，并在并行修复。此报告与 initial 探针记录保留原发现；修复后要重新构建探针/测试，不能拿旧二进制结果断言新代码仍失败或已通过。

bits-code-guard start.py 已执行，初始化 telemetry 因 DNS 解析失败跳过；diff_and_filter.py 已执行并补全 untracked 范围。HTML/Markdown 由最终缺陷 JSON 生成；finish.py 按流程执行。未打开浏览器、未发布外部评论、未 stage。临时详细报告：[report.html](/tmp/qt-video-workbench_m8_spec_review/report.html)。
