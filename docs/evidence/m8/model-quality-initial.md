# T037 ModelClient 代码质量审查

审查时间：2026-10-02 21:48:16 +0800。结论：**FAIL，发现 1 项 P2 健壮性问题**；未发现其他达到报告标准的 P0–P2 缺陷。现有假 CLI 测试独立运行 **19 passed / 0 failed**。

## 评审范围与流程

范围仅为 `app/agent/ModelClient.h`、`app/agent/ModelClient.cpp`、`tests/unit/ModelClientTest.cpp`、`tests/agent-model.cmake`，共 4 文件、581 行。四文件均为 untracked，已逐个按完整文件审查；`git diff HEAD` 默认不包含这些文件，因此补充 `/dev/null` 到当前文件的新增侧 diff。没有 stage、源代码修改、重构建或外部评论。

先读取 `AGENTS.md` 和 `model-spec-review.md`。规格复审的 PASS 已在质量审查前存在，本报告不重新定义规格完成状态。读取直接调用方仅用于确认接口和返回值语义，不对其另行审查。

使用 bits-code-guard，完成通用七维度检查：逻辑、业务语义、安全、并发、健壮性、性能和代码质量。C++/CMake 没有该 skill 的语言专项规则。按 ≤5 文件的聚合规则保留单组，本地串行审查。最终结果已做新代码方向、行号、范围、分级、置信度、语义去重及最多五项限制的自检。

## 问题统计

| 严重级别 | 数量 |
| --- | ---: |
| P0 | 0 |
| P1 | 0 |
| P2 | 1 |
| 合计 | 1 |

## 1. [P2][健壮性问题] 启动阶段取消后若启动失败，退役进程与临时目录不会释放

位置：`app/agent/ModelClient.cpp:73–81`。置信度：**9/10**。

具体触发路径：在 macOS/Unix 上把程序设为不存在或无法执行的路径，调用 `request()`，并在事件循环处理启动失败之前调用 `cancel()`、替换请求或析构客户端。进程仍处于 `Starting`，清理函数将它脱离父对象并断开客户端的错误处理连接；请求目录只由 `finished` 回调捕获。

```cpp
if (oldProcess->state() != QProcess::NotRunning) {
    oldProcess->setParent(nullptr);
    auto retiredDirectory = std::shared_ptr<QTemporaryDir>(std::move(directory));
    QObject::connect(oldProcess, &QProcess::finished, oldProcess,
        [oldProcess, retiredDirectory] { oldProcess->deleteLater(); });
    oldProcess->kill();
}
```

Qt 6.11.2 的 `_q_startupNotification()` 启动失败分支会切换到 `NotRunning`、发出 `errorOccurred(FailedToStart)`、清理内部资源并返回，**不发出 `finished`**。因此上面的唯一删除回调不会执行，QProcess/QTimer 和捕获的 QTemporaryDir 留存；反复触发会积累对象及 schema/instructions 请求文件。此判断来自当前代码与固定版本 Qt 官方源码，未声称已经运行新增时序探针。[Qt 6.11.2 启动失败分支](https://github.com/qt/qtbase/blob/v6.11.2/src/corelib/io/qprocess.cpp#L1168)、[Qt 6.11.2 Unix 异步启动及内部清理](https://github.com/qt/qtbase/blob/v6.11.2/src/corelib/io/qprocess_unix.cpp#L672)。

定级依据：已确认影响限于本地退役对象和请求文件残留，触发需启动窗口内取消及启动失败同时成立，未确认核心流程故障或凭据外泄，按影响范围定为 P2。

修复建议：退役进程同时处理 `finished` 和 `errorOccurred(FailedToStart)`，用幂等回收函数，在确认停止后 `deleteLater()`，让临时目录随最终进程对象回收释放；保留非阻塞清理。添加无效路径请求后立即取消、替换和析构的回归测试，确认退役对象销毁及目录删除。

现有 `failedStartAndTimeout` 会等待 `FailedToStart` 后再继续；取消测试使用可以启动的假 CLI。两者未覆盖“已取消的 Starting 进程随后启动失败”的组合。

## 已验证结果与可用限制

本次运行现有隔离测试二进制，未重构建：

```text
.workbench/model-client-test/build/model_client_test
Qt 6.11.2 / macOS 15.7.7
Totals: 19 passed, 0 failed, 0 skipped, 0 blacklisted, 5191ms
exit code: 0
```

运行前检查二进制修改时间晚于四个审查文件，并核对实现/测试 hash 与规格复审快照一致。原始日志：`/tmp/qt-video-workbench_t037_model_quality_20261002/model-client-test.log`。

参数数组与 stdin 传输、禁止危险绕过参数、请求文件隔离、 stdout/stderr/result 上限、工具 item 拒绝、非零退出、畸形/非对象/空对象结果、超时、普通取消、旧回调及信号重入处理得到静态和模拟验证。`completed` 表示收到非空 JSON 对象，领域方案仍需 PlanService 等调用方验证。

管理员配置仍可能提供工具。**ModelClient 的禁用配置加工具事件检测不构成对管理员所有工具的绝对隔离**；事件检测只能在观测到工具事件后停止请求和拒绝结果，不能证明工具没有先发生副作用。

旧版客户端的真实结构化请求已获 `qvw.agent-plan@1` / `kind=clarify`，但发生于最后一次记忆禁用修复前。**本次仅运行假 CLI，不访问真实模型；最终修复版仍需独立真实请求复测**，也不代表完整视频创作闭环已验收。

## 工具自检与 telemetry

bits-code-guard `start.py` 已调用，初始化及失败报告 telemetry 均因 DNS 解析失败跳过（`nodename nor servname provided, or not known`）；未当作 telemetry 成功。`diff_and_filter.py` 已调用，但只能看见范围外的两个 CMake 跟踪文件，均被过滤，因此明确补充四个 untracked 源文件并设置 `scope: full_file`。最终报告由 `generate_report.py` 生成；`finish.py` 按要求在报告生成后调用，也因相同 DNS 失败跳过，原始结果记录于 `/tmp/qt-video-workbench_t037_model_quality_20261002/finish.log`。

查阅 Qt 时 ego-browser 因 sandbox 内无法连接 `ego_cli bootstrap` 退出；随后通过 web 只读访问 Qt 官方文档和固定版本源码成功。没有申请 Full Access 或操作用户浏览器。

## 快照 SHA-256

```text
04a588436bced70010b76b2f272392a432ed4e347fb16b2d8b8acbc7e784cb23  app/agent/ModelClient.h
edc21d2c062ac6866e6a774e5a64d71625d42c451525d9051ed938a3235c5341  app/agent/ModelClient.cpp
8218e784a00d20275fd4e190336311067cf2222c112ca92db625d5a3f6c93763  tests/unit/ModelClientTest.cpp
b92c90efd91e25389ced152b071e4e2abb4f20f0e672f7210fa2bf37d97976ab  tests/agent-model.cmake
```

详情：[model-quality-initial.html](model-quality-initial.html) ｜ [model-quality-initial.json](model-quality-initial.json)。
