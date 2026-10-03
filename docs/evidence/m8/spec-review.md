# M8–M10 Agent 创作闭环规格审查

**最终规格复核与交付验收证据：PASS。当前未发现未关闭的规格缺陷。** 初审5项缺陷、后续范围/选择/诊断/创建身份问题及质量审查3项P1均已关闭。质量修复后完整CTest **31/31（166.84秒）**，1.2.0包的图片/视频实际预览与清理、搬移9案例均通过。真实2次模型请求的AgentE2E **3/3**发生在这3项质量修复之前；本次仅读取修复源码与现有证据，没有运行新测试、Build或模型请求。提交与推送由主任务处理。

依据 `AGENTS.md`、已批准的 `docs/plans/2026-10-02-agent-creation.md` 与用户创作闭环要求。本次不改源代码、不stage、不spawn；独立通过的ModelClient原范围不重复纳入；本次仅追加其环境API、QProcess传环境与main配置接线的有限核对。审查范围为新增Agent服务、AgentPlan、Snapshot/authoredId与mapper、story-reel模板、AgentPanel/PlanReviewPanel、MainWindow/main、共享Editor版本处理和相关测试；新增可视预览确认亦已纳入。

## 需求覆盖与证据性质

| 要求 | 已核对行为 | 证据 |
|---|---|---|
| 工程事实、Qt选中、真实能力进入模型 | ContextBuilder只包含实际Project/Snapshot字段、登记素材、实际selectedEntity及独立editScope；无像素和preview HTML | 源码；Controller独立目标复验 |
| 本地三图片、默认15秒竖屏可编辑三卡 | 默认720×1280/30fps、90/240/120帧；卡片编辑素材、文字、排序及整数帧时长，显示总时长 | UI25项；模板测试；最新真实E2E |
| 严格类型与授权 | 顶层/场景/操作键严格；拒绝未知/重复字段、错误类型、越scope、过期基线和未登记素材；批准由Qt生成SHA | PlanService/ProposalService/ApprovalManager源码及负向测试 |
| 完整方案一次确认 | main先更新并校验用户编辑的整个方案，内容相等且canApprove才执行；创建显示新路径与工程切换，不覆写现有目录 | main接线；UI确认/过期弹窗测试 |
| 顺序执行与真实回读 | 经同一Document/Editor，每次一个写入；operationSucceeded后字段回读，确认版本后推进；同fp两次有界刷新消化上游迟到revision | 共享Editor25项；Controller23项；真实E2E15项创建 |
| 失败、重试、停止、修订 | 成功前缀保留；失败步骤最多重试2次；停止只挡后续；repair依据当前事实和持久化真实失败，等待新批准 | Controller协议模拟；独立8项目标复验 |
| 多轮只改结尾 | 第二次真实Codex按实际结尾实体提出字段修改，前两段保持；共享Undo/Redo使用同一历史 | 最新真实E2E；真实E2E通过（质量修复前） |
| 重开与任务事实 | 有界JSON/JSONL、QSaveFile；禁止所有路径段symlink；反复stale不改保存基线；恢复scope和错误；旧批准不恢复 | 独立恢复探针；原TaskStore6项/最终11项；Controller23项 |
| 当前任务范围保持不变 | 下一次scope与task scope分离；approve取plan.base；thinking/apply/failed导航不改原执行scope，下一次generate使用新范围 | 独立8项目标复验，含成功前缀及失败后重试 |
| 新工程身份和编译等待 | 创建信号立即保存新root、未知revision/指纹及新工程空scope；restore等同root真实Editor，不能用旧工程快照 | 合法创建root RED；独立8项目标复验 |
| 工程/可视预览/导出版本 | 编译快照单独显示；中央iframe内容与当前编译HTML及已解码媒体一致后才确认revision/fp；最近导出按实际fp显示过期 | 原WebEngine16项、质量修复后33项；UI版本测试；最终打包Studio启动验收 |
| 实际导出验收 | 用户选路径，经ExportController冻结当前fp/version、观察真实Build ID、获取实际Output，再参数与全片解码验收 | 最新真实E2E；真实E2E通过（质量修复前） |
| 有界进度和能力边界 | 显示阶段与完成条目，无百分比、无原子事务承诺；停止观察不当作取消Build；不开放模型源码/Shell/URL或收费生成 | 源码；Controller/UI/ProductWindow测试 |

上表“协议模拟”包含假模型可执行文件和localhost HTTP响应，不能替代真实Codex/Hypit证据。已有视频模板字段沿用真实Snapshot/ProposalService/Editor能力路径；本轮尚无专门的真实模型视频编辑用例，不把它写成已经实测。配音、字幕、任意剪辑和像素理解属于后续扩展。

## 缺陷关闭记录

初审失败证据完整保留于 [spec-review-initial.md](spec-review-initial.md)、[spec-review-initial.json](spec-review-initial.json)、[spec-review-probe-initial.log](spec-review-probe-initial.log)。

| 问题 | 修复后结果 | 结论 |
|---|---|---|
| P1 工程内symlink事件文件会写坏源码 | `eventAppend=0 authoredSourceMutated=0`；TaskStore内部/外部文件与目录链接拒绝 | 关闭 |
| P1 stale恢复覆盖保存基线，第二次可批准 | 两次均stale且不可批准；保存fp不变 | 关闭 |
| P1 restore遗漏限定scope | status.scope和plan.base.scope保留ending，剩余步骤需新确认 | 关闭 |
| P1 编译Snapshot冒充可视预览确认 | 编译与可视版本分开；真实DOM、媒体及revision/fp匹配才确认，失败/导航撤销 | 关闭 |
| P2 损坏任务静默 | failed状态和信号；原损坏文件不覆盖 | 关闭 |
| 附加：恢复后下一轮scope与未勾checkbox不一致 | generate先同步当前可见scope，后发送goal | 独立UI+Controller探针关闭 |
| 附加：真实Qt选择与授权scope混用 | 独立selectionChanged接线，ContextBuilder分离selectedEntity/editScope；父Track/clear清空选择 | 独立Controller与MainWindow复验关闭 |
| 附加：导航覆盖当前任务/执行scope | task范围保持，approve和后续确认使用原批准范围；persist保存task scope | 独立Controller复验关闭 |
| 附加：创建编译前保存旧executionRoot | 新工程创建立即建立新目标基线；未知编译版本不伪造；失败诊断保留 | 合法RED与独立Controller复验关闭 |
| 附加：失败诊断重开丢失 | lastResult/错误和操作索引/entity/field/valueHash有界保存，restore后进入repair上下文 | 独立Controller复验关闭 |

独立恢复探针 [spec-review-probe-recheck.log](spec-review-probe-recheck.log)，TaskStore [spec-task-store-recheck.log](spec-task-store-recheck.log)，共享Editor [spec-editor-allowed-recheck.log](spec-editor-allowed-recheck.log) 25/25。首次Editor运行因受限沙箱不能监听localhost而失败，原日志 [spec-editor-recheck.log](spec-editor-recheck.log) 保留；批准后重跑通过，没有把环境拒绝算成功。

最终稳定主构建上独立运行6个Controller目标案例，加init/cleanup共8/8：[spec-scope-root-diagnostics-final.log](spec-scope-root-diagnostics-final.log)。覆盖实际选择、thinking/apply导航、新root、失败后导航/仅重试剩余步骤、诊断恢复修订。MainWindow真实选择父Track的独立复验共3/3：[spec-selection-parent-final.log](spec-selection-parent-final.log)。全Controller [controller-final.log](controller-final.log) 23/23，完整MainWindow [selection-parent-green.log](selection-parent-green.log) 12/12，UI [ui-selection-green.log](ui-selection-green.log) 25/25。

创建Root的有效失败证据是 [controller-creation-root-red.log](controller-creation-root-red.log)：旧`/project`与新`/new-project`身份不同。早期sceneId错误或路径规范化不一致导致的fixture失败另保留，不作为Root行为缺口的有效RED。

## 质量修复后的有限规格复核（2026-10-03）

[quality-review.md](quality-review.md) 的最终独立质量结论为PASS、当前缺陷清单为空。此轮按主任务要求只读取3项修复和现有最新证据；表中独立复验由质量审查员执行，本次没有重跑它们。

| 质量审查P1 | 有限源码核对 | 已记录修复后证据 |
|---|---|---|
| TaskStore特殊文件可同步阻塞或被替换 | path在QFile/QLockFile前逐段拒绝symlink，既有父项必须目录、终点必须普通文件；覆盖current/events/lock/轮转端点 | [task-special-green.log](task-special-green.log) 全11 PASS；[quality-task-special-recheck.log](quality-task-special-recheck.log) 独立7 PASS，FIFO保留 |
| Codex缺少应用配置子进程环境 | ModelClient保存环境并在start前setProcessEnvironment；main初始和配置reload均传AppConfig.processEnvironment，未更改父PATH | [model-environment-green.log](model-environment-green.log) 全23 PASS；[quality-model-environment-recheck.log](quality-model-environment-recheck.log) 独立3 PASS |
| 合法固定文本运行时布局被误判为旧预览 | 授权仅来自inert expected DOM；仅2种运行时属性和5项shrink几何样式例外，校验scale/geometry/line值，继续严格比较文字、字体、素材和其他属性 | [preview-text-layout-green.log](preview-text-layout-green.log) 全33 PASS；[quality-preview-text-recheck.log](quality-preview-text-recheck.log) 独立19 PASS |

原RED、首轮质量报告与修复前31套回归仍保留；未把修复前的真实模型/导出运行改写成修复后新执行。环境补充只核对配置传递，原ModelClient的隔离、协议和取消逻辑继续引用独立模型与质量审查。

## 实际可视预览

固定上游契约已核对：`shared.ts:286`、`session.ts:106`、`ui/stage.ts:349–355,365–371`、`preview/runtime-shim.ts:211,234–235`。Transport比对中央iframe的实际文档、静态内容/代码/材质、编译HTML、资源解码及运行时加载状态；仅srcdoc属性已写入、旧文档、页面失败、Artifact模式或旧异步回调均不能确认。

[preview-version-green.log](preview-version-green.log) 16/16是质量修复前的Qt WebEngine DOM协议模拟；初始12项未覆盖字面`%n`与被新seek取代的初始化Promise，追加失败保留于 [preview-edge-red.log](preview-edge-red.log)。修复接受已settle而被新seek取代的false，Promise拒绝仍不确认；图像/视频加载状态仍需通过。UI [preview-ui-green.log](preview-ui-green.log) 24/24。

[preview-startup.json](preview-startup.json) / [preview-startup.log](preview-startup.log) 是主应用真实启动固定Hypit并打开22:40三场景工程的证据：imagesReady/mediaReady/previewMatchesSnapshot均true，预览v1、fp `2c756f3173e3e1ebc6def4e85066f61f757915e09f9fff67d387454107d36fd2`。验收代码真实等待window.previewVersion与当前Snapshot匹配。这是22:40旧Run的实际可视预览历史证据；最新Run的最终打包预览证据如下，不再存在待核验项。


最终1.2.0包 [packaged-agent-startup.json](packaged-agent-startup.json) 真实打开23:40最新E2E工程：imagesReady/mediaReady/previewMatchesSnapshot均true，预览v1、fp `0933ec9ff70e0e156e7de4566d4a15a54177992a4107d1f8d87c09b562b2f6ad`，与该Run工程和成片记录一致。原视频工程 [packaged-video-startup.json](packaged-video-startup.json) 同样真实loaded，previewVideosReady/mediaReady/previewMatchesSnapshot均true，fp `bfd9fda5d876f4059e188e8efaeee838a03349898b97607995379b709628687d`。两份startup均exitCode0、cleanupStopped=true；[packaged-native.json](packaged-native.json) 还确认自有Studio进程已退出、初始最小PATH、中文空格启动目录、移除Qt环境覆盖和无截图。

[relocation.json](relocation.json) 的9案例PASS：4种搬移启动均exit0并清理自有Studio；缺Hypit/版本错误预期exit6，缺Node/FFmpeg/ffprobe预期exit4。此为本机搬移/缺依赖验收，`otherMachineVerified=false`；Hypit仍是外部依赖，未打包进应用。

## 最终真实闭环证据

质量3项修复后最终完整 [ctest-final.log](ctest-final.log)：31/31，166.84秒；覆盖全部受影响门禁及新增preview_version。修复前31/31、174.37秒的运行单独保留于 [ctest-before-quality-fixes.log](ctest-before-quality-fixes.log)。此前29/30的旧UI链接失败原样保留于 [ctest-before-ui-relink.log](ctest-before-ui-relink.log)，没有覆盖或计为通过。

[agent-e2e-final.log](agent-e2e-final.log)：3/3，69860ms；此真实运行在3项质量修复之前，未重新请求模型或构建视频。[agent-e2e.json](agent-e2e.json) 记录两次当前Codex登录的真实模型请求、15项创建、三张不同本地背景、默认90/240/120帧、只修改结尾、共享Undo/Redo、重开任务及当前导出的参数/全片解码。首次成功运行仍保留于 [agent-e2e-run.log](agent-e2e-run.log)，它不是这次最终运行日志。

先前独立读取该最新Run产物、工程与任务状态：[spec-latest-artifact-readback.json](spec-latest-artifact-readback.json)。文件真实存在（99425字节），task的Build ID、exportFingerprint和artifact均与最新E2E一致：

- Build：`bld_20261002T154129631Z_0EF9376F68`
- 最终fp：`0933ec9ff70e0e156e7de4566d4a15a54177992a4107d1f8d87c09b562b2f6ad`
- 工程：`.workbench/agent-creation-20261002-234036-336/校园故事工程/`
- 产物：`.workbench/agent-creation-20261002-234036-336/校园创作社-15秒.mp4`
- 媒体：720×1280、30fps、450帧/15秒；ExportController参数检查与全片解码通过。

重开后的任务保持stale，因为共享Redo已改变此前保存的执行基线；它没有继承旧批准。最近导出字段仍指向当前最终fp。ExportController自身也会由ExportWorkspace恢复其已完成任务，UI按该任务与当前Snapshot指纹判断过期。

E2E由测试驱动updatePlan/approve，`testDriverConfirmsPlan=true`；UI确认交互另有Widgets测试，不声称用户在真实GUI上亲手批准。媒体参数及全片可解码不等于创作内容符合目标，`semanticVisualVerdict=requires human viewing`；没有截图验收或上传图片像素。未重复模型请求、Build或媒体解码来充当独立复验；独立复验核对真实文件与控制器记录的一致性。

## 结论与范围

本轮三场景图片创建、限定多轮编辑、共享历史、恢复新批准、失败/重试/停止/修订、实际可视预览版本及当前输入真实导出的规格门禁通过；当前无已确认且未关闭的实现缺陷。当前源码SHA登记在 [spec-review-sources-final.json](spec-review-sources-final.json)：原54文件范围，加3个仅环境集成的有限ModelClient补充，共57文件、5583行文本身份；此计数不表示重审ModelClient全部实现。原规格快照54文件/4777行保留于 [spec-review-sources-before-quality.json](spec-review-sources-before-quality.json)。

证据边界：真实2次模型E2E发生于3项质量修复前；其后由针对性环境/文件/真实DOM复验、全31回归及最终包的图片/视频native和9案例搬移验收覆盖。专门的真实模型视频模板编辑用例未新增；主观视觉/文案验收需人工；未验证另一台机器。质量与打包验收已关闭，提交/推送由主任务继续处理。22:40历史预览与23:40最终包预览分别对应各自工程指纹，没有混写。

质量修复前的规格复验技能报告（历史）：[report.html](/tmp/qt-video-workbench_m8_spec_recheck/report.html) / [report.md](/tmp/qt-video-workbench_m8_spec_recheck/report.md)。初审问题与原始失败证据已保留，最终源码结论不抹去历史缺陷。
