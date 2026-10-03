# T037 ModelClient 代码质量复审

复审时间：2026-10-02 21:54:58 +0800。结论：**PASS，首轮 1 项 P2 健壮性问题已修复；当前未发现未关闭的 P0–P2 缺陷**。首轮报告已原样保留为 [model-quality-initial.md](model-quality-initial.md)、[model-quality-initial.json](model-quality-initial.json)、[model-quality-initial.html](model-quality-initial.html)。

## 复审范围

原范围为 `app/agent/ModelClient.h`、`app/agent/ModelClient.cpp`、`tests/unit/ModelClientTest.cpp`、`tests/agent-model.cmake`。当前共 4 文件、642 行，仍全部为 untracked；已补充完整文件范围，没有依赖默认 `git diff HEAD` 漏掉新文件。本次聚焦首轮 P2 的 `dispose()` 修复及新增回归测试，未扩展到其他 Agent 实现。

`AGENTS.md` 与规格复审 PASS 已在首轮质量审查前读取。本次使用 bits-code-guard 的范围、自检、结构化清单和 HTML 工作流；C++/CMake 无专用语言规则。首轮七维度结果作为基线，本次检查修复的逻辑、生命周期、资源所有权、信号重入和测试可靠性。没有修改源代码、stage、重构建或外部评论。

## 修复核对

首轮具体条件为：macOS/Unix 上使用不存在或无法执行的程序路径，调用 `request()` 后，在启动失败事件处理前取消、替换或析构客户端，退役进程仍为 `Starting`；后续 `FailedToStart` 不发 `finished`，导致旧的单一 finished 回收路径失效。固定版本契约依据：[Qt 6.11.2 启动失败分支](https://github.com/qt/qtbase/blob/v6.11.2/src/corelib/io/qprocess.cpp#L1168)。

| 核对项 | 当前实现或测试 | 结果 |
| --- | --- | --- |
| 启动失败也回收退役进程 | `ModelClient.cpp:85` 连接 `finished`；`:88` 连接 `errorOccurred`，在 `FailedToStart` 时调用同一 `reap` | 已关闭 |
| 两种回调不重复安排销毁 | `ModelClient.cpp:79–83` 共享 `deletionScheduled`，先置位再 `deleteLater()` | 满足 |
| 请求文件与子定时器按所有权释放 | `ModelClient.cpp:78–80` 的回调捕获 `retiredDirectory`；目录保持到退役进程最终回收，定时器仍以进程为父对象 | 满足 |
| cancel / replace / destroy 清理 | `ModelClientTest.cpp:203–246` 显式确认 `Starting`；每种动作检查临时目录删除、QProcess 与子 QTimer 的 QPointer 置空、无旧 failed/completed；replace 只完成新请求 | 三组通过 |
| RED 证据确实捕获原缺陷 | `model-starting-red.log` 中 cancel 与 destroy 在目录删除断言失败；replace 当次通过，符合子进程失败与 kill 的调度差异 | 2 失败的原始证据保留 |

`reap` 在进程对象自身的线程执行；`deleteLater` 使 Qt 的启动失败内部清理可以先返回。目录由同一退役进程连接持有，在最终对象回收时释放。该修复覆盖了原本缺失的 `FailedToStart` 路径，保留非阻塞清理。

## 验证证据

读取实现者日志：`model-starting-red.log` 为 **3 passed / 2 failed**（包含初始化、清理）；修复后完整 `model-green.log` 为 **22 passed / 0 failed，4082 ms**。本次未回退源代码重演 RED，也未重跑完整套件。

本次独立运行现有隔离二进制的新增测试：

```text
.workbench/model-client-test/build/model_client_test reclaimsCancelledStartingFailure

PASS : reclaimsCancelledStartingFailure(cancel)
PASS : reclaimsCancelledStartingFailure(replace)
PASS : reclaimsCancelledStartingFailure(destroy)
Totals: 5 passed, 0 failed, 0 skipped, 0 blacklisted, 169ms
exit code: 0
```

5 项中的另外两项为 QtTest 初始化和清理。运行环境为 Qt 6.11.2 / macOS 15.7.7；运行前检查二进制修改时间晚于实现和测试源码。源码 hash 在报告生成前再次核对一致，首轮 initial 三个报告 hash 也保持一致。原始本次日志：`/tmp/qt-video-workbench_t037_model_quality_recheck_20261002/model-starting-test.log`。

当前缺陷统计：P0 = 0、P1 = 0、P2 = 0，结构化清单为 `[]`。

## 可用限制

本次通过表示首轮代码质量问题已关闭。上述回归测试使用假 CLI，并在本机 macOS/Qt 6.11.2 上执行；没有验证所有平台的进程启动时序，也没有访问真实模型。最终修复版真实模型可用性需以独立实际请求证据判断，不能由本质量报告替代；完整视频创作闭环也需各自验收。

`completed` 表示收到非空 JSON 对象，领域方案仍需 PlanService 等调用方校验。管理员配置仍可能提供工具。**ModelClient 的禁用配置加工具事件检测不构成对管理员所有工具的绝对隔离**；事件检测只能在观测到工具事件后停止请求和拒绝结果，不能证明工具此前没有副作用。

## 工具自检与 telemetry

本轮 `start.py` 已调用，初始化及失败报告 telemetry 均因 DNS 解析失败跳过（`nodename nor servname provided, or not known`）。`diff_and_filter.py` 已调用，默认结果只能看见范围外跟踪文件；本轮明确补充 untracked 文件，设置 `scope: full_file`。最终清单已做范围、新代码侧、行号、去重和关闭状态自检；`generate_report.py` 生成 HTML。`finish.py` 按流程在报告生成后调用，原始结果记录于 `/tmp/qt-video-workbench_t037_model_quality_recheck_20261002/finish.log`。未把网络失败当作 telemetry 成功。

## 当前快照 SHA-256

```text
04a588436bced70010b76b2f272392a432ed4e347fb16b2d8b8acbc7e784cb23  app/agent/ModelClient.h
3a9623028907ac43a92e247b6f79197d54cfa6908e1fd543527ae64b3f201749  app/agent/ModelClient.cpp
0576ae39edbe9e56abf2bd05679b85821463856b894ccd12da1cba48ca14c408  tests/unit/ModelClientTest.cpp
b92c90efd91e25389ced152b071e4e2abb4f20f0e672f7210fa2bf37d97976ab  tests/agent-model.cmake
```

详情：[model-quality-review.html](model-quality-review.html) ｜ [model-quality-review.json](model-quality-review.json)。
