# M8–M10：FrameLab 1.2.0 创作闭环证据

本机macOS15.7.7 arm64、Qt6.11.2、固定Hypit0.2.10；Codex0.159.2使用当前ChatGPT登录。模拟协议、真实模型、真实执行和发布验证分别留证据。本轮不以截图验收。

## 最终构建与测试

| 检查 | 结果 | 证据 |
|---|---|---|
| 完整CTest | 三质量修复后31/31 PASS，166.84秒 | [ctest-final.log](ctest-final.log) |
| 有界ModelClient | 最终23项PASS，含配置环境、启动中取消、旧回调、超限/畸形结果、超时与工具事件拒绝 | [model-environment-green.log](model-environment-green.log) |
| 方案与模板 | Plan7项、Story Qt3项、Node2项PASS | [plan-green.log](plan-green.log)、[story-green.log](story-green.log)、[story-template-node-green.log](story-template-node-green.log) |
| 控制器负向协议 | 23项PASS：成功前缀、停止、有限重试、外部改动、恢复、作用范围和新工程根 | [controller-final.log](controller-final.log) |
| 任务目录保护 | 最终11项PASS，包含内部目录符号链接及FIFO拒绝，不阻塞或覆盖特殊文件 | [task-special-green.log](task-special-green.log) |
| 共用编辑历史 | 25项PASS；同源码watcher重发版本保留历史 | [history-republish-green.log](history-republish-green.log) |
| Qt交互 | 面板25项、窗口12项PASS：选中事实独立、未来范围、父轨道/取消选择清空 | [ui-selection-green.log](ui-selection-green.log)、[selection-parent-green.log](selection-parent-green.log) |
| 实际DOM版本确认 | 最终33项PASS，确认当前源码、载入媒体与Runtime帧就绪；合法文字缩放/行布局可确认，静态改动/旧DOM/损坏媒体不确认 | [preview-text-layout-green.log](preview-text-layout-green.log) |
| 显式Codex定位 | 24项RuntimePaths回归PASS，支持Finder启动配置`tools.codex` | [codex-path-green.log](codex-path-green.log) |

## 真实多轮创作

[agent-e2e-final.log](agent-e2e-final.log)：3/3 PASS，69.860秒；[agent-e2e.json](agent-e2e.json)记录最新工程、成片、Build ID和源码指纹。

两次真实模型请求：三张原创本地测试背景→15秒三段卡片→修改开场文案→本地确认创建→15项真实编辑回读→只改选中结尾标题→共享撤销和重做→关闭重开→导出最新版本。测试驱动调用生产控制器批准方案，未声称人工点击验收。媒体经过ExportController的参数检查与全片解码；[film-ffprobe.json](film-ffprobe.json)再次记录本地交付副本的实际规格。

成片720×1280、30fps、450帧、15秒，H.264。模型只接收元数据，未上传图片像素；无配音。样例图片是标有FRAME的原创背景图。媒体规格和执行通过不等于视觉内容已由人审阅。

较早的独立真实主GUI检查见[preview-startup.json](preview-startup.json)，它核验22:40创建工程的DOM与源码指纹。最新端到端运行使用23:40的另一独立工程，不能混用两次运行的指纹。

## 修复与历史失败

- 目录链接可覆盖源码：`task-symlink-red.log`→`task-symlink-green.log`。
- Hypit目录watcher迟到导致连续写入409：`agent-e2e-watcher-race.log`、`history-republish-red.log`→真实E2E及共享历史GREEN；每步确认后两次间隔刷新仅接纳同源码版本增长。
- 停止/撤销后的编译未稳定：`controller-stop-settle-red.log`、`controller-undo-settle-red.log`→控制器最终GREEN。
- 失败诊断恢复丢失、实际选择和授权混用、忙时导航改变任务范围：`controller-diagnostics-red.log`、`controller-selection-red.log`、`controller-scope-root-red.log`前两项、`controller-failed-scope-red.log`→最终GREEN。
- 新工程编译前记录旧工程根：合法方案的`controller-creation-root-red.log`明确读到旧`/project`，最终GREEN。`controller-scope-root-red.log`第三项为非法scene ID fixture失败，`controller-creation-canonical-fixture-red.log`为`/var`与`/private/var`路径规范化fixture失败，两者均不计入该缺陷的有效RED。
- 父Track仍传旧Clip：`selection-parent-red.log`→`selection-parent-green.log`；未限制scope时漏传actual selection：`ui-selection-red.log`→`ui-selection-green.log`。
- 原始初轮CTest29/30记录在`ctest-before-ui-relink.log`，新增UI用例当时链接了旧库；最终顺序构建后31/31通过。
- 最初E2E中的GUI application、模板重复order、authoredId与完整ID、撤销后立即重做问题分别保留在`agent-e2e-initial.log`、`agent-e2e-template-order-failure.log`、`agent-e2e-entity-failure.log`和`agent-e2e-undo-race.log`；均未计作PASS。
- 早期模型探针`real-model-probe.json`保留历史边界说明，最终Client真实请求以E2E和[模型规格复核](model-spec-review.md)为准。

## 审查与发布验收

[规格复核](spec-review.md)、[质量复核](quality-review.md)均PASS。首轮质量3项P1完整保存在[初审报告](quality-review-initial.md)：特殊任务文件阻塞、CLI缺应用环境、合法文字Runtime布局误判。各项RED/GREEN及独立复验记录分别为`task-special-*`/`quality-task-special-recheck.log`、`model-environment-*`/`quality-model-environment-recheck.log`、`preview-text-layout-*`/`quality-preview-text-recheck.log`。修复前完整31/31（174.37秒）保存在`ctest-before-quality-fixes.log`，最终31/31为修复后的166.84秒。

两次真实模型与成片E2E发生在上述三处质量修复前，最终相关负向/正向门禁和发布包在修复后复验；未重复模型和Build来替代质量回归。最新版运行包核验最新工程的中央预览，与这份成片源码指纹一致：[packaged-agent-startup.json](packaged-agent-startup.json)。原视频工程也由新包实际解码并确认版本：[packaged-video-startup.json](packaged-video-startup.json)。二者都在中文空格非仓库CWD、最小初始PATH/无Qt路径覆盖下运行，退出后自有Studio不存在：[汇总](packaged-native.json)。原生验收脚本首轮读错`previewSourceFingerprint`字段，实际应用报告为PASS；改为真实`previewFingerprint`后两例完整通过，未将该脚本字段错误计成产品故障。

发布过程先完整部署Qt，再从最终源码重新构建Release并复制最新主程序、执行`--repair-existing`检查/签名/归档；修复模式自身不编译。证据：[release-build-final.log](release-build-final.log)、[package-final.log](package-final.log)、[release.json](release.json)、[link-audit.json](link-audit.json)。82个实际Mach-O、严格本地签名、248份许可输入与manifest一致；ZIP SHA及交付成片与原导出一致：[delivery-integrity.json](delivery-integrity.json)。

搬移9种场景均符合预期：四种默认/显式Node/官方shell的实际成功，五种缺依赖/错版本正确非零FAIL，[relocation.json](relocation.json)。外部Hypit使用本机固定Distribution，没有随Qt包vendor；本地ad hoc签名，未公证，另一台机器和Windows未验收。

G8–G10及T037–T042依据以上实际证据完成。交付入口`.workbench/启动FrameLab.command`、成片`.workbench/deliverables-m8/校园创作社-15秒.mp4`，可编辑工程路径见真实E2E JSON。
