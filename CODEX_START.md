# 首轮交给 Codex 的提示词

将下面内容粘贴到项目根目录中的 Codex 会话。它是给开发工具的任务，不是桌面产品内部 Agent 的系统提示词。

```text
请在当前目录实施 Qt Video Workbench 项目。

先读取 AGENTS.md、PROJECT_PLAN.md、STATUS.md、tasks.json，以及 docs/API_CONTRACT.md 和 docs/UPSTREAM_EVIDENCE.md。
先检查已有文件和 git 状态，不覆盖已有规范、代码或未提交修改。

本轮只执行 M0（T001–T005）：
1. 检查当前 OS、编译器、CMake、Qt Kit（含 WebEngine）、Node、pnpm、FFmpeg/ffprobe、浏览器和网络条件，填写环境报告。
2. 获取并固定实际使用的 Hypit 版本。读取源码与 --help，记录 SHA 或包版本/integrity，禁止猜接口和参数。上游材料中观察到的 0.2.12 仅作线索。
3. 优先检查官方 examples/semantic-composition/chat 示例，准备只使用本地渲染的 Runtime，在依赖准备经授权后，完成 check、plan、真实 build 和 get。
4. 在临时工程验证 Studio 地址发现、GET session、一个实际可写属性的 mutation、冲突和失败行为。不要为制造可写属性而编造字段。
5. 用最小 Qt WebEngine 探针打开实际 Studio，并检查画面；记录目标媒体格式兼容性。测试 Qt 原生 HTTP 的本地 Host/Origin 行为。
6. 将成功证据、失败命令、退出码与未验证项写入 docs/ENVIRONMENT_REPORT.md 和 docs/COMPATIBILITY_REPORT.md，更新任务与状态文件。

限制：本轮不实现完整 UI，不做 Agent，不接付费模型，不自动安装大型依赖，不修改全局环境或隐藏失败。如果缺少权限、网络、依赖或平台能力，明确报告阻塞项；可编写独立探针和测试，但不要将真实集成标为通过。

M0 完成后停止，给出 G0 的 PASS / PARTIAL / BLOCKED、已生成证据路径，以及下一轮应执行的任务。
```

## 第二轮开始

使用 `prompts/NEXT_ITERATION.md`，明确写入一个任务 ID，例如 `T006`。任务没有完成时优先修复；不要绕过前置门禁连续扩功能。

## 新会话恢复

使用 `prompts/RESUME.md`。进度以仓库状态与测试证据为准，不以此前会话口头声称为准。

## 为什么根目录有 AGENTS.md

官方文档说明 Codex 会读取项目指导文件；本包仅把稳定约束放进 AGENTS.md，大计划按需读取，避免指令文件膨胀。具体加载与覆盖规则以官方资料 [S15] 为准。不要为使用本计划修改全局 Codex 权限或关闭审批。
