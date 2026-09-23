# Studio 接口契约

**契约来源版本：`@hypit/hypit` 0.2.10，git commit `1af179d3f58284c2d6d3c1f63052172a4fe1b5a6`。**

本文件的每条断言都来自以下两类证据之一，二者缺一不写入：
1. 固定版本的上游源码（给出文件与行号）；
2. 本机对运行中 Studio 的实测（给出状态码与响应体）。

Hypit 升级后本文件必须重新验证，不得沿用。

## 1. 启动与地址发现

Studio 是本地进程，不是常驻服务。启动形式（实测可用）：

```bash
./hypit studio --run <run.svrun> \
  --workspace <project-dir> \
  --runtime <hypit.runtime.json> \
  --port <number>
```

`--workspace` 与 `--runtime` 对示例工程是**必需**的，省略会因无法解析已安装包而失败（实测错误：`cannot locate installed package @example/chat-scene`）。

地址发现：**以 `--port` 显式指定为准**，并从进程 stdout 解析确认行。stdout 会打印 Project、Run、Runtime Profile、Runtime selection 与 `Local: http://localhost:<port>/`。

不存在"默认端口"约定。Qt 侧应自行选择空闲端口、显式传入，并以 stdout 的 URL 行作为就绪信号，不得轮询猜测端口。

## 2. 端点清单

路由来源：`packages/studio/src/server.ts`。

| 方法 | 路径 | 用途 | 行号 |
|---|---|---|---|
| GET | `/__studio/session` | 读取当前 Snapshot | 622 |
| GET/HEAD | `/__studio/visual.html` | 已编译预览 HTML | 600 |
| GET | `/__studio/document` | 预览文档的 JSON 形式 | 611 |
| GET | `/__studio/library?section=tasks\|artifacts` | 任务与产物列表 | 638 |
| GET | `/__studio/surface-preview` | Surface 预览图 | 659 |
| GET | `/__studio/storyboard/<res_id>` | 故事板资源 | 683 |
| GET | `/__studio/material/<res_id>` | 素材资源 | 717 |
| GET | `/__studio/artifact` | 产物文件 | 748 |
| POST | `/__studio/mutation` | 结构化属性/时间轴写入 | 561 |
| PUT | `/__studio/source` | 整文件源码写入 | 480 |
| PUT | `/__studio/artifact-name` | 重命名产物 | 535 |

三个写入端点（480、535、561）在进入 body 校验**之前**先过跨源门禁。

## 3. GET /__studio/session

实测：HTTP 200，28440 字节（chat 示例）。

**响应体本身就是 Snapshot，没有 `{data:...}` 包装。** 实测顶层键：

```
revision, source, run, space, tracks, preview, provenance
```

已用 Qt 原生请求断言 `wrappedInData=false`。Qt 解析层不得预期任何外层信封。

类型定义见 `packages/studio/src/shared.ts:255`：

```ts
type StudioSnapshot = {
  revision: number;
  source: { path: string; text: string; files: StudioSourceView[] };
  run: { path: string; targets: string[]; satisfactions: {output,candidate}[] };
  script?: ScriptMap;
  space: { canvasWidth; canvasHeight; clearColor; frameRate:{numerator,denominator};
           frameCount; durationSec };
  tracks: Track[];
  semantic?: SemanticTimeline;
  preview: { kind: "hyperframes"; srcdoc: string };
  provenance: { picture: "resolved"; note: string };
};
```

实测 chat 示例的值：`revision=1`，`space` 为 540×960 / 30:1 fps / 240 帧 / 8 秒，`tracks` 长度 1，`source.files` 为三项（`chat.svrun:run`、`chat.svml:author`、`chat.svs:dependency`）。

`source.files` 是 Run + Author 闭包，源码注释明确说明"never a directory scan or inferred project tree"。Qt 不应把它当作项目文件浏览器的数据源。

`space` 的画幅时长与最终导出的 MP4 实测完全一致（540×960、30fps、8s），故可作为导出预期的依据。但这些值来自模板声明，**不是 Hypit 全局默认值**。

### 失败响应

无法编译时返回 HTTP 500，body 为 `StudioFailure`（`shared.ts:326`）：

```ts
{ revision: number; error: string; range?: Range }
```

`range` 仅在解释器定位到出错元素时存在。Qt 的错误展示应当容忍其缺失。

## 4. 可写字段的发现方式

可写字段**不是** Snapshot 的顶层概念，需沿 `tracks[].clips[].inspector[]` 逐项读取。

字段类型 `StudioInspectorField`（`packages/studio-adapter/src/index.ts:217`）：

```ts
{
  id: string;
  control: StudioParameterControl;
  value: CanonicalValue;
  binding?: string;
  schema?: ValueSchema;
  edit?: { language; source; attributes? };
}
```

**判定可写的唯一依据是 `edit` 字段存在。** `control` 只描述展示形态，不代表可写。

字段之所以可写，源头在组件的 Studio facet 中声明了 `{ name, writable: true }`（`StudioSourceBindingDeclaration`，同文件 231 行）。仓库内已有此类声明的包：`packages/ranking-studio`、`packages/media-track-studio`、`packages/performance-studio`、`packages/audio-track-studio`、`packages/sound-studio`、`packages/deck-track-studio`、`packages/screen-overlay-studio`、`examples/complex-explainer/packages/*/src/studio.js`。

时间轴字段是例外：其可写性来自 Runtime 权威而非组件 allowlist（`packages/studio/src/parameters.ts:503` 注释明确指出这点）。

### 实测警告

chat 示例的唯一 clip **`inspector` 为空数组**，三个 `editHandles`（move / trim-start / trim-end）全部 `enabled: false`，`disabledReason` 为"该时间表达未开放时间轴回写"。

所以"组件能渲染"与"字段可编辑"是两件事，这与 PROJECT_PLAN §4.3 的警告一致。**Qt 属性面板必须按 `edit` 是否存在决定控件可用性，对不可写字段展示只读，并优先显示 `disabledReason` 而不是自造提示文案。**

## 5. POST /__studio/mutation

请求体为 `StudioMutation`（`shared.ts:295`），只有两种形态：

```ts
| { type: "parameter.adjust"; revision: number; entityId: string;
    parameterId: string; value: CanonicalValue }
| { type: "timeline.adjust"; revision: number; entityId: string;
    gesture: StudioTimelineGesture; target: {kind:"instant",frame,...}
                                          | {kind:"window",startFrame,endFrameExclusive,...} }
```

源码注释称这是"The complete author mutation vocabulary exposed by Studio"，即除此之外没有其他结构化写入手段。

`revision` 必须等于当前 Snapshot 的 revision。成功时响应 `{ revision: <新值> }`，新 revision 是递增后的值。

### 实测状态码矩阵

curl 与 Qt `QNetworkAccessManager` 两条路径结果一致：

| 场景 | HTTP | body |
|---|---|---|
| 畸形 body | 400 | `Expected a Studio author mutation.` |
| 跨源 Origin | 403 | `Cross-origin Studio mutations are prohibited.` |
| revision 过期 | 409 | `The Source changed outside Studio.` |
| entity 不存在 | 500 | `Studio entity <id> no longer exists.` |

**未验证：成功写入（HTTP 200）路径。** 原因是本轮找不到含可写字段的纯本地示例，且按项目约束不伪造字段。这是 G0 判 PARTIAL 的唯一原因。

### 服务端的失败回退语义

`commitMutation`（`server.ts:440`）的实际行为，逐步对照源码：

1. 若已有 mutation 在进行中 → 抛错，不排队。**服务端一次只接受一个写入。**
2. 校验 `mutation.revision === snapshot.revision` 且 `requestedRevision === snapshot.revision`，否则报 `The Source changed outside Studio.`（第 443 行）。
3. 计算 patch 并 `applyTransaction`，保留 `previous` 预像。
4. 递增 revision 并重新编译。
5. **若重编译失败：调用 `replaceSourceFiles(previous)` 回滚，再次编译，然后抛 `StudioMutationRejected`**（第 462–466 行），HTTP 422。

即 `/__studio/mutation` 这一条路径上，编译失败会由服务端自动还原源文件。

**但这不能推广到 `PUT /__studio/source`。** 该端点（第 480 行起）虽然也调用 `applyTransaction` 并带 `preimage`，但其编译在 `publish(attempt)` 之后**没有**对应的回滚分支，成功即返回 202。PROJECT_PLAN §8 关于"不能一概承诺自动还原"的判断在此得到源码印证：两条写入路径的失败语义不同。

因此本项目仍需自建单文件预像、重编译验证与冲突恢复，不依赖服务端回滚。

## 6. 跨源门禁（安全相关）

来源：`packages/studio/src/mutation-origin.ts`，三条全部为放行前提：

1. `Host` 的 hostname 必须是 `localhost`、`127.0.0.1` 或 `[::1]`，且 `target.host === host.toLowerCase()`；
2. `Origin` 若存在，其 `origin` 必须与 target 同源；
3. `sec-fetch-site` 若存在，必须是 `same-origin` 或 `none`。

Qt 的 `QNetworkAccessManager` 默认不发送 `Origin`，也不发送 `sec-fetch-site`，只发 `Host`。按上述规则，条件 2、3 因头部缺失而不触发，条件 1 由 `localhost:<port>` 满足。

**实测确认：Qt 原生请求返回 400（body 校验阶段的错误）而非 403，证明已通过门禁。** 这是 Qt 原生写入路径可行的前提。

Qt 侧不得主动添加 `Origin` 头；一旦添加且不同源即被 403 拒绝（已实测）。

## 7. PUT /__studio/source

请求体：`{ text: string; revision: number; path?: string }`。

约束（源码第 500–516 行）：
- 三个字段类型不符 → 400 `Expected source path, text and revision.`
- revision 不匹配 → 409 `The Source changed outside Studio.`
- `path` 省略时默认为当前 Source 的 workspace 相对路径；
- **`path` 若为绝对路径，或不在 `allowedSourceFiles` 集合内 → 抛错**（`Studio cannot write source file <path>.`）。这是路径逃逸防护，Qt 侧应只提交 Snapshot `source.files` 中出现过的相对路径。

成功返回 **202** `{ revision }`，注意不是 200。

## 8. 对 Qt 实现的硬性约束

1. 解析 session 响应时**不要**假设 `{data:...}` 信封。
2. 端口显式传入，以 stdout 的 URL 行为就绪信号。
3. 调用任何 CLI/Studio 命令必须同时带 `--workspace` 与 `--runtime`。
4. 属性控件的可用性以 `inspector[].edit` 是否存在为准，禁止以 `control` 推断。
5. 不可写字段展示 `disabledReason` 原文。
6. 写入前后都要重新 GET session 核对 revision，不缓存旧 revision 重试。
7. 409 与 422 必须走不同的恢复流程：409 是外部改动需重建历史起点，422 是本次编辑被编译拒绝且服务端已回滚。
8. 不主动设置 `Origin` 头。
9. 服务端串行写入，Qt 侧需自行排队，不可并发提交 mutation。
10. `PUT /__studio/source` 成功码是 202，不是 200。
