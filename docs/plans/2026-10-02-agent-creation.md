# Agent 创作工作台实施计划（M8–M10）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. 用户已提供并批准产品方案，要求继续实现；沿用当前独立 qt-video-workbench 分支与外部 ../hypit 契约。

**Goal:** 本地图片与自然语言目标经真实 Codex 生成可审阅方案，确认后创建三段竖屏工程；继续局部修改、恢复任务并导出当前版本。

**Architecture:** Agent 仅提出类型化方案；Qt 持有版本、批准范围和执行结果。异步 ModelClient 调用当前 Codex 登录，ContextBuilder 读取真实 Snapshot，PlanService 校验，AgentController 经同一 DocumentController / EditorController / ExportController 顺序执行。工程事实、任务 JSON 与追加事件分别保存。

**Tech Stack:** C++17 / Qt 6 Core、Widgets、QProcess；固定 Hypit 0.2.10；原创 SVML 图片故事模板；现有本地媒体验收。

## 已批准设计与边界

以用户附件的“Agent 驱动的视频创作工作台”方案为产品要求，采用当前 Codex 登录。第一条闭环为无配音的三段图片短片，默认720×1280、30fps、15秒、3/8/4秒。模型只接收用户选择素材的名称、尺寸、类型、ID和工程字段，不上传图片像素、不声称看过画面；不接收费素材生成服务。

创建方案包含三张卡片，支持修改文字、选择素材、改时长及顺序。总时长显式显示；已声明保护范围或总时长相互冲突时返回澄清，不能偷偷更改其他场景。运行后的局部编辑只开放真实 Snapshot 的可写属性；任意源代码、Shell、外部URL或未登记素材不开放。

模型返回 create / edit / clarify 三类结构化方案。请求基线和批准记录由程序产生，模型不能写批准。确认时重新校验工程路径、版本、指纹、字段类型、素材登记和选中作用域。创建允许关闭当前工程后切换到用户选择的新目录，确认弹窗展示这个动作；目标目录不得覆写现有工程。

执行不是事务：逐步记录已完成操作和每步确认版本；失败保留成功前缀并允许在同一指纹下重试失败步骤。停止只阻止下一步，不回滚已确认写入或取消已提交Build。人工编辑使待执行方案暂停；同一 Editor 的撤销按实际历史逐步回退。重开读取工程事实和任务记录，重新审阅剩余动作，绝不恢复过期批准。

任务进度展示实际阶段和完成条目，不编造百分比。显示工程版本、预览确认版本和最近导出版本/是否过期；导出复用冻结输入、真实Build ID观察和参数/全片解码验收；媒体规格通过不等于内容符合创作目标。

## T037 / M8：真实 ModelClient

Files: `app/agent/ModelClient.{h,cpp}`, `tests/unit/ModelClientTest.cpp`, `tests/agent-model.cmake`。

- [x] 新增独立异步接口 `request(QString prompt,QJsonObject schema)` / `cancel()`，信号 `completed(QJsonObject)`、`failed(QString)`、`busyChanged(bool)`。
- [x] 用假CLI验证参数、stdin、JSON结果、非零退出、畸形/超限输出、超时、取消和旧回调；先运行RED，后实现GREEN。
- [x] QProcess 参数数组调用 `codex exec --ephemeral --ignore-user-config --ignore-rules --sandbox read-only --output-schema ... --output-last-message ... --json -`；独立QTemporaryDir，清理受控临时输出。保留登录，禁止读取/输出凭据。关闭shell/apps/hooks/web/multi-agent/remote-plugin，不启用危险绕过参数。
- [x] 本地认证检测与实际结构化请求分别留证据；模拟CLI通过不算真实模型接通。核对官方non-interactive-mode、developer-commands、config-reference与实际0.159.2 help。

## T038 / M8：ContextBuilder 与 PlanService

Files: `app/domain/AgentPlan.h`, `app/agent/ContextBuilder.{h,cpp}`, `app/agent/PlanService.{h,cpp}`, `tests/unit/AgentPlanTest.cpp`。

- [x] 定义严格schema `qvw.agent-plan@1`，create / edit / clarify，最多3场景/24操作，拒绝额外键和混合行为；本地维护基线。
- [x] 上下文从 Project、Snapshot、Qt选择和只读素材目录生成；给出真实字段ID、类型、当前值、兼容绑定和精确能力；声明未读取像素。
- [x] 复用 ProposalService 校验每个参数操作；整张方案确认前先校验所有操作，不因后续逐项版本增长失效自己的操作。
- [x] 属性scope选择可由用户取消限定；scope内只允许选中实体。素材路径必须来自登记ID，不允许模型提供任意路径。
- [x] 测试错误类型、越权/未知字段、重复操作、过期指纹、未登记素材、三段顺序/总时长和需澄清请求；记录RED/GREEN。

## T039 / M9：原创三段模板与安全创建

Files: `templates/story-reel/`, `app/agent/StoryTemplate.{h,cpp}`, `tests/template/story-reel.test.mjs`, `tests/unit/StoryTemplateTest.cpp`。

- [x] 原创720×1280图片故事场景布局与三个真实可写组件；标题、副标题、图片、主题色、字号由author-kit和Studio companion声明。
- [x] 创建时由程序把已校验场景时长/顺序生成为受控模板实例，SVML文本严格转义。仅复制安装的可信模板，不使用模型代码；动态时长限制3–60秒、每段1–30秒/整数帧。
- [x] 所有图片先经现有AssetService不可变导入，SHA登记；等实际Studio Snapshot/Editor ready后逐项填字段、回读确认。失败保留真实工程和进度。
- [x] 实测固定Hypit编译、三个clip跨度、可写属性与预览；先Node/Qt测试RED再GREEN，真实Studio另留证据。

## T040 / M9：AgentController / Approval / ToolDispatcher

Files: `app/agent/AgentController.{h,cpp}`, `app/agent/ApprovalManager.h`, `app/agent/ToolDispatcher.{h,cpp}`, `tests/unit/AgentControllerTest.cpp`。

- [x] 用户目标→读取上下文→真实模型→待审阅方案；生成不改变工程。确认记录规范JSON的SHA、当前项目身份/版本/指纹；任何方案修改需重新校验。
- [x] 程序受控工具：读取上下文、提交方案、可信创建、应用已批准编辑、刷新预览、启动导出、读取构建状态、验收产物。通过现有控制器，不复制编辑/历史/构建逻辑。
- [x] 每次只发一个写入，operationSucceeded+Snapshot真实值确认后记录新基线并推进；最多24步、失败步骤最多重试2次。手动改动/项目切换/未知成功停住并回读，不重放整个成功前缀。
- [x] 模型可根据一次执行失败和新事实提出修订方案，仍需新确认；有界请求与实际日志可审阅。

## T041 / M10：任务持久化与多轮交付

Files: `app/agent/AgentTaskStore.{h,cpp}`, controller/export integrations, `tests/unit/AgentTaskStoreTest.cpp`。

- [x] 工程 `.workbench/agent/` 保存有界任务JSON、追加JSONL事件：task/project ID、用户目标、方案hash、scope、成功前缀、新revision/指纹、失败/重试、Build ID和实际产物。QSaveFile原子保存，禁止符号链接/路径穿越。
- [x] 重开读取真实Snapshot再恢复可查看状态；匹配才允许重新批准剩余步骤。过期/损坏记录明确提示，不自动执行；关闭取消模型请求和后续编辑。
- [x] 导出由用户明确点击/选择路径，复用ExportController；Build状态程序观察，不模型空转。最近输出与当前指纹比较，正确显示过期。

## T042 / M10：Qt 协同界面与真实验收

Files: `app/ui/AgentPanel.{h,cpp}`, `app/ui/PlanReviewPanel.{h,cpp}`, `app/ui/MainWindow.{h,cpp}`, `app/main.cpp`, `tests/unit/AgentPanelTest.cpp`, `tests/integration/AgentE2ETest.cpp`。

- [x] Agent成为主入口；显示当前作用范围、素材选择、目标输入、实际模型/阶段、方案卡片或修改差异、确认/停止/重试/撤销/恢复；保留手动属性与旧模拟提案在辅助入口并清楚标识。
- [x] 增加Qt组件选中实体信号；创建卡片可编辑素材/文字/时长/顺序，确认前展示总时长和新目录。UI不持有QProcess或直接改源码。
- [x] 真实端到端：三张本地图片→当前Codex生成15s方案→编辑卡片→确认创建→核验三个场景→Codex只修改结尾→撤销/重做→重开任务/工程→导出最新15s竖屏H264并全片解码。无截图验收。
- [x] 负向实测：确认前源码不变、人工修改使旧方案失效、越权拒绝、部分失败记录正确、停止后续、重试不重做成功前缀、旧成片提示过期。
- [x] CTest受影响门禁及完整回归、规格审查→代码质量审查→修复后复验；更新STATUS、tasks、使用指南、证据后提交并推送分支。

## 后续扩展

M7已具备视频素材真实导入与模板能力，Agent可编辑该模板真实暴露的字段。配音、字幕对齐和像素级素材理解不在本轮能力列表；每项需独立工具和真实验收再开放，不能借模型措辞宣称实现。
