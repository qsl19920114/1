# T037 ModelClient 规格审查

审查时间：2026-10-02 18:24 +0800。结论：**FAIL，存在 2 项规格缺口**。现有模拟测试通过；真实 Codex 结构化请求尚未验收，因此本报告不能作为 T037 真实模型接通的完成证明。

范围仅为 `app/agent/ModelClient.h`、`app/agent/ModelClient.cpp`、`tests/unit/ModelClientTest.cpp`、`tests/agent-model.cmake`，并读取仓库 `AGENTS.md`、T037 计划及 `model-red.log` / `model-green.log` 作为约束和证据。未修改上述源代码，未审查其他 Agent 实现，也未进行后续代码质量审查。

## 规格缺口

### 1. 未明确关闭记忆功能

- 位置：`app/agent/ModelClient.cpp:202`；`tests/unit/ModelClientTest.cpp:138`。
- 严重度：P2；置信度：9/10。
- 要求：关闭记忆及文档读入。
- 现状：参数只设置 `history.persistence="none"`，没有关闭 `features.memories`，也没有设置 `memories.use_memories=false` / `memories.generate_memories=false`。对应模拟断言重复同一参数集合，因此没有检测这一遗漏。
- 依据：本机 `codex-cli 0.159.2` 的 `codex features list` 列出独立的 `memories` 功能。当前环境显示该功能为 `false`；这只说明当前默认状态，不能证明 ModelClient 强制关闭了记忆。官方配置说明中，`history.persistence` 控制 `history.jsonl` 保存，`memories.use_memories` 控制已有记忆注入，`features.memories` 是独立开关。[OpenAI Configuration Reference](https://learn.chatgpt.com/docs/config-file/config-reference)
- 修复建议：为每次请求显式关闭记忆功能、注入和生成，并在参数模拟测试中逐项断言。继续保留管理员配置可能提供工具的边界说明，不能据此声称绝对隔离。此项发现没有声称本次已发生记忆泄漏。

### 2. 空对象结果被当作成功

- 位置：`app/agent/ModelClient.cpp:268`；`tests/unit/ModelClientTest.cpp:74`、`:155`。
- 严重度：P2；置信度：10/10（已实测行为）。
- 要求：非零退出、畸形、非对象及空结果应拒绝。本次按空结果包含空 JSON 对象解释此要求。
- 现状：客户端仅验证 JSON 解析成功且为对象；`{}` 会直接触发 `completed({})`。现有 `empty` 模式只写入零字节文件，没有测试空对象。
- 复现：在 `/private/tmp/qvw-t037-spec-probe/empty-object.cpp` 创建独立审查探针，链接本次构建的 `qvw_agent` 库。假 CLI 正常退出、输出有效 JSONL，并写入 `{}` 结果文件。运行 `/private/tmp/qvw-t037-spec-probe/empty-object`，输出 `completed: empty=true`，退出码 0。探针未改仓库源代码、未调用真实模型。
- 修复建议：取得 `document.object()` 后拒绝 `isEmpty()`，为假 CLI 增加空对象模式并验证只触发一次 `failed`，不触发 `completed`。

## 逐项核对

| 规格 | 核对结果 | 实现与测试证据 |
| --- | --- | --- |
| `qvw::agent` 内 QObject；`request` / `cancel` / `setProgram` / `setTimeoutMs` / `isBusy`；三类信号 | 满足 | `ModelClient.h:7`；模拟测试编译并监听三种信号 |
| 异步 QProcess、参数数组与 stdin | 满足 | `ModelClient.cpp:212`、`:226`、`:283`；测试 `runsAsynchronouslyWithIsolatedArgumentsAndStdin` 检查立即 busy、异步完成及含中文/命令字符的原样 stdin |
| 当前 Codex 登录，不复制凭据 | 实现符合；真实认证未验收 | 默认程序 `codex`，未覆盖 `CODEX_HOME` 或复制/读取认证文件；本机 `codex exec --help` 明确 `--ignore-user-config` 保留 `CODEX_HOME` 认证。模拟不能证明真实认证可用 |
| ephemeral / ignore-user-config / ignore-rules / read-only / skip-git / schema / output-last-message / json / stdin | 满足 | `ModelClient.cpp:194`；测试 `:132` 检查参数与 schema / result 路径 |
| 独立 QTemporaryDir、自有 instructions 与清理 | 满足 | `ModelClient.cpp:174`、`:180`、`:182`、`:209`、`:218`；测试读取自有 instructions、检查工作目录及 schema / result 同目录，完成后目录不存在 |
| 关闭 shell / unified_exec / apps / hooks / web / multi-agent / remote-plugin / 文档读入 | 满足 | `ModelClient.cpp:202` 参数逐项关闭；测试 `:138` 检查，`:139` 禁止危险绕过参数 |
| 关闭记忆 | **缺口 1** | 当前默认关闭与请求显式禁用是不同证据层级 |
| 不伪称管理员工具已被绝对隔离；工具 item fail closed | 满足 | `ModelClient.cpp:111` 明确边界；任意 `item.*` 的 item.type 仅允许 `agent_message` / `reasoning`，其他类型停止；模拟覆盖 command_execution、未知 item、分段 mcp_tool_call。只证明观测到事件后拒绝结果，不能证明管理员工具从未执行 |
| stdout 256 KiB / stderr 128 KiB / result 64 KiB | 满足 | `ModelClient.cpp:15`、`:127`、`:257`；三种超限模拟均拒绝，不发 completed |
| 默认 120 秒超时；可设置测试超时 | 满足 | `ModelClient.cpp:39`、`:223`、`:282`、`:295`；模拟设置 30 ms，验证超时 failed |
| 取消、替换请求与旧回调抑制 | 满足 | generation + process 身份检查、断开旧回调、异步 kill；模拟覆盖 cancel/restart、忙时替换及 busyChanged 重入时抑制旧 completed |
| 非零 / 畸形 / 非对象 / 缺失 / 空文件结果拒绝 | 满足 | `ModelClient.cpp:248`、`:253`、`:268`；对应模拟全部通过 |
| 空 JSON 对象拒绝 | **缺口 2** | 补充探针返回 `{}` 时 completed；现有模拟未覆盖 |
| 错误不回显 token | 满足 | 实现只产生固定错误及数字退出码，不回显 stdout/stderr；nonzero 模拟 stderr 含测试 Bearer token，断言 failed 不含 token / Authorization |
| 测试接入 CMake / CTest | 满足 | `tests/agent-model.cmake:1` 注册 `model_client` 并设 20 秒测试超时；`tests/CMakeLists.txt:81` include 对应文件 |

## 验证记录与真实性边界

TDD 留存日志：`model-red.log` 为 **2 passed / 16 failed**，`model-green.log` 为 **18 passed / 0 failed**。这些日志支持所记录的 RED → GREEN 过程；未在本次审查中回退实现重演 RED。

本次独立执行：

```text
build/tests/model_client_test
Totals: 18 passed, 0 failed, 0 skipped, 0 blacklisted, 3750ms
exit code: 0
```

运行前检查到测试可执行文件修改时间晚于 `ModelClient.cpp` 与 `ModelClientTest.cpp`。这 18 项全部通过测试可执行文件自身实现的 `fakeCodex` 完成，未访问真实 Codex 模型、未验收当前登录、网络、Schema 服务执行或真实模型的最终结果。补充空对象探针也属于模拟验证。

本次仅执行 `codex --version`、`codex --help`、`codex exec --help`、`codex features list` 等本机只读查询，确认 CLI 为 0.159.2 及参数/功能开关信息；没有读取或输出认证凭据。真实结构化请求仍需独立实测证据。

审查快照 SHA-256：

```text
04a588436bced70010b76b2f272392a432ed4e347fb16b2d8b8acbc7e784cb23  app/agent/ModelClient.h
1c44f73dc7ec68222b515ac694092bd6200663088a03ea667a969fcb2550a6ce  app/agent/ModelClient.cpp
4c86b02f9a11acbb031b110a2018ea71b0c84c18a83bd188b51f0eb7f385d33b  tests/unit/ModelClientTest.cpp
b92c90efd91e25389ced152b071e4e2abb4f20f0e672f7210fa2bf37d97976ab  tests/agent-model.cmake
```
