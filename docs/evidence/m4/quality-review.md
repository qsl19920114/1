# M4 质量复审

结论：PASS。修复后未发现剩余确定 P0–P2 缺陷，质量审查可交付。

范围：基准 `d060619` 加当前全部 tracked / untracked M4 源码、UI、测试及 CMake，共 21 个文件、1848 行完整文件内容。未来阶段计划不在本轮范围内。按 bits-code-guard 三组串行 fallback 完成逻辑、业务语义、安全、并发、健壮性和性能检查；thread limit 阻止新 reviewer 委派。

## 本轮发现并关闭

| 初始级别／置信度 | 问题及触发 | 位置 | 修复与回归 |
|---|---|---|---|
| P1／10/10 | 停止尚未返回 ID 的提交并重开后，resume 通过 activity/builds 找到唯一 ID。该分支 publish 触发 taskChanged；直接槽在 working 且 ID 非空时调用 cancelBuild，已启动 cancel 子进程。外层 publish 返回后继续 send(status)，导致 JsonProcess::start 返回 false；fail 会杀掉 cancel 并停止观察。正常 acceptBuild 分支已有 process.isRunning 防护，此恢复分支缺失。 | app/controllers/ExportController.cpp:149 | 增加 publish 后 process.isRunning guard，保留重入已启动的 cancel。 回归：tests/unit/ExportControllerTest.cpp:136 |
| P1／10/10 | 用户可将一次成片导出到工程根目录 export-stage.mp4。下次修改源码后再次导出，freeze 的 collect/hash 会复制这个普通文件到冻结工作区。固定 stagePath 同样是 workspace/export-stage.mp4；get 会移除并写入新视频，改变冻结清单记录的输入哈希。此时停止或重开，validateRecovered 拒绝合法任务并无法恢复原 Build。 | app/services/ExportWorkspace.cpp:122 | stagePath 改为独立隐藏 .export-stage.mp4，普通 export-stage.mp4 保持冻结输入哈希。 回归：tests/unit/ExportControllerTest.cpp:147 |
| P2／9/10 | cancel 返回 requested=false 时先 emit message，再无条件 acceptBuild。若 message 的直接槽调用 clearProject 或 setProject，generation 已变化且旧观察已停止，但旧回复仍修改当前 task 的 ID、phase 和 active，可能把已关闭或新工程的状态改回旧 Build。当前主窗口槽只记录日志，因此按防御性不足定 P2；此路径与其他消息分支采用的 generation 隔离不一致。 | app/controllers/ExportController.cpp:158 | 取消提示信号返回后检查 generation 与 busy，丢弃已关闭工程的旧回复。 回归：tests/unit/ExportControllerTest.cpp:142 |

## 验证证据与限制

- 三条初始 RED 日志：`controller-unknown-cancel-red.log`、`controller-cancel-message-red.log`、`controller-stage-collision-red.log`。
- 修复后 `controller-quality-regressions-green.log`：三个针对性回归 PASS，7739 ms。
- `workspace-green.log`：9 个 QtTest case PASS。
- 最后质量修复前完整 15 项 CTest PASS（90.46 秒）与真实 E2E PASS（20.985 秒）；真实计划失败显示 `CLI_ERROR：Unexpected token ';'`，无 Build ID、无交付、无 completed。
- 最终根目录构建 exit 0；`ctest-quality-final.log` 中受影响 `export_controller` / `export_workspace` 顺序 CTest 2/2 PASS，49.30 秒。`ctest-concurrent-build.log` 使用旧 binary 的失败已被保留，未计入成功证据。
- 本审查代理只读源码和已有证据，未运行测试，未修改生产代码，未发送外部评论，未打开报告。
- mandatory `start.py` 已调用，遥测因 DNS 不可用自动跳过；`finish.py` 已调用，遥测同样因 DNS 不可用自动跳过。

报告：`quality-review.json`、`quality-review.html`。
