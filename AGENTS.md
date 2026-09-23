# AGENTS.md

本文件是给开发工具（Codex / Claude 等）的稳定约束。大计划按需读取 `PROJECT_PLAN.md`，不要把计划全文搬进本文件。

## 项目是什么

基于 Qt 6 Widgets 的桌面视频创作工作台。本项目自己实现工程与素材管理、模板实例化、原生属性编辑、版本安全、异步任务管理与交付验收。视频语言、编译和渲染能力复用 Hypit；浏览器预览先复用现成 Studio。

本项目**不是**重写剪映，**不是**给网页套窗口。

## 目录与上游的关系

本仓库只包含本项目原创内容。Hypit 是外部依赖，通过 `config/version-lock.json` 的 `distributionPath` 指向本机检出（默认 `../hypit`），**不 vendor 进本仓库**。

原因：上游 LICENSE 带附加条件（见 PROJECT_PLAN §10），且答辩需要清楚区分原创与复用。

## 不可违反的约束

1. **不猜接口。** 任何关于 Hypit / Studio 的断言必须来自固定版本的源码（给出文件行号）或本机实测（给出状态码与输出）。已验证的契约在 `docs/API_CONTRACT.md`，不要绕过它自行假设。
2. **不伪造通过。** 真实集成未验证就不能标 done。模拟通过与真实可用必须分开记录。缺权限、缺网络、缺依赖就明确报阻塞项。
3. **成功语义分层。** HTTP 接受、CLI 退出、Build 完成、Output 获取、成片可解码是五件不同的事。任何一层失败都不得显示"导出成功"。
4. **门禁顺序。** 前置任务未完成时优先修它，不要跳过门禁连续扩功能。门禁定义见 PROJECT_PLAN §7 与 `tasks.json` 的 `gates`。
5. **不改全局环境。** 不修改 shell 配置、不污染 PATH 依赖。构建时用 `-DCMAKE_PREFIX_PATH` 显式指定 Qt。
6. **不接付费 Provider。** 选示例和 Runtime Profile 时先核对 `hypit.runtime.json` 的 endpoints 是否全为本地。
7. **不为制造可写属性而编造字段。**

## 调用 Hypit 的已知约定

这些是 M0 实测得出的，照做即可，不要重新发明：

```bash
# 必须同时传 --workspace 和 --runtime，否则无法解析已安装包
<hypit> <cmd> <path>/x.svrun \
  --workspace <example-dir> \
  --runtime <example-dir>/hypit.runtime.json

# Studio 端口显式指定，以 stdout 的 URL 行为就绪信号；不存在默认端口约定
<hypit> studio --run ... --workspace ... --runtime ... --port <n>
```

## Studio 契约要点

完整版见 `docs/API_CONTRACT.md`。最容易踩的四条：

- `GET /__studio/session` 的响应体**就是** Snapshot，没有 `{data:...}` 包装。
- 字段可写与否看 `inspector[].edit` 是否存在，**不看** `control`。
- 409（外部改动）与 422（编译拒绝且服务端已回滚）必须走不同恢复流程。
- Qt 不要主动设 `Origin` 头，否则被跨源门禁 403。

## 状态以什么为准

仓库状态与测试证据。此前会话的口头声称不算。恢复工作时读 `STATUS.md` 和 `tasks.json`，然后自己核对证据文件是否真的存在。
