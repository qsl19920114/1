# M0 环境报告

任务：T001–T005｜执行日期：2026-09-23｜执行方式：本机真实命令，全部退出码与输出留档于 `/tmp/m0/`。

本文件只记录**实测结果**。未实际执行的项目一律标注"未验证"，不做推断。

## 1. 主机与工具链（T001）

| 项目 | 实测值 | 取值命令 |
|---|---|---|
| 操作系统 | macOS 15.7.7（BuildVersion 24G720） | `sw_vers` |
| 架构 | arm64（Apple Silicon） | `uname -m` |
| 编译器 | Apple clang 17.0.0（clang-1700.0.13.5），target arm64-apple-darwin24.6.0 | `clang++ --version` |
| CMake | 4.4.3（本轮经 Homebrew 安装，原先缺失） | `cmake --version` |
| Qt | 6.11.2，前缀 `/opt/homebrew/opt/qt`（本轮经 Homebrew 安装，原先缺失） | `qmake6 -query QT_VERSION` |
| Node | v22.22.1 | `node -v` |
| FFmpeg / ffprobe | 9.0.2 | `ffmpeg -version` |
| Homebrew | 7.0.4 | `brew --version` |

环境变更记录：本轮执行 `brew install cmake qt`，属经用户授权的本机依赖安装。未修改任何全局 Qt 环境变量，未改动 shell 配置；构建时通过 `-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt` 显式指定，不依赖 PATH 污染。

### Qt Kit 组件可用性

`/opt/homebrew/opt/qt/lib/cmake` 下确认存在项目计划所需的全部 CMake 配置包：

`Qt6Core`、`Qt6Widgets`、`Qt6Network`、`Qt6Test`、`Qt6WebEngineCore`、`Qt6WebEngineWidgets`、`Qt6WebEngineQuick`。

`QtWebEngineProcess.app` 存在于 `QtWebEngineCore.framework/Helpers`，即 WebEngine 的独立渲染进程已随包提供。

结论：**PROJECT_PLAN §5 设定的 Qt 最低目标 6.8 在本机以 6.11.2 满足，且 WebEngine Widgets 可用。** 本机即可作为主平台。

未验证：Qt 6.8–6.10 是否与同一 Studio 行为一致（只测了 6.11.2）；其他操作系统的 Kit 可用性。

### pnpm 状态

`pnpm` 命令不在 PATH，`corepack` 也不在当前 node 安装中。但这**不构成阻塞**：`node_modules/` 与 `dist/` 已存在于仓库检出中，`./hypit` 启动器可直接运行（见下节实测）。本轮未安装 pnpm。

影响范围：`pnpm --filter ... build` 这类重新编译上游包的操作目前无法执行。若后续需要自建 Author Package（M2 的 title-card 模板），需要先补 pnpm。**这是 M2 的前置阻塞项。**

## 2. Hypit 版本固定（T002）

| 项目 | 实测值 |
|---|---|
| 包名 | `@hypit/hypit` |
| 版本 | **0.2.10** |
| git commit | `1af179d3f58284c2d6d3c1f63052172a4fe1b5a6` |
| Distribution 路径 | `/Users/bytedance/Downloads/国产资产作业/hypit` |
| 启动器 | `bin/hypit.mjs` |
| 声明的 Node 要求 | `>=22.15.0`（本机 22.22.1 满足） |
| 声明的 packageManager | `pnpm@10.33.0` |

取值命令：`./hypit version --json`、`git rev-parse HEAD`、`package.json`。

**修正上游材料的线索：CODEX_START.md 提到的 0.2.12 与实际不符，本项目固定在 0.2.10 + commit `1af179d3`。** 后续所有接口断言均以此版本的源码为准。

CLI 命令面（`./hypit --help` 实测）：authoring 有 `check/plan/pricing/build`；results 有 `builds/history/logs/status/inspect/get/result edit`；runtime 有 `doctor/runtime/programs/packages/activity/cancel/paths/auth`；另有 `studio`、`media`、`capture`、`transcribe/measure/snapshot`。

`./hypit paths` 实测的状态位置：项目状态 `<project>/.hypit`，宿主状态 `~/Library/Application Support/Hypit`，机器包目录 `~/Library/Application Support/Hypit/packages`。

## 3. 官方本地示例真实闭环（T003）

选用 `examples/semantic-composition`（chat 示例）。选择依据：其 `hypit.runtime.json` 只声明 `@hypit/provider-media-local` 与 `@hypit/provider-hyperframes-local` 两个本地端点，**无任何付费 Provider**，符合本轮"不接付费模型"的限制。已核对其余示例：`interview`/`podcast`/`ranking-football` 均含 `hypihub.default`（托管 Provider），故排除。

### 关键调用约定（实测踩坑）

不带 `--workspace` 直接在仓库根目录调用会失败：

```
x Command failed
  CLI_ERROR
  cannot resolve installed package @example/chat-scene from <repo root>:
  cannot locate installed package @example/chat-scene
```

正确形式必须把 workspace 指向示例目录本身：

```bash
./hypit <cmd> examples/semantic-composition/chat.svrun \
  --workspace examples/semantic-composition \
  --runtime examples/semantic-composition/hypit.runtime.json
```

这一点对 M1 的"Qt 自动启动 Studio"有直接影响：**Qt 侧拼装命令行时必须同时传 `--workspace` 与 `--runtime`，不能只传 Run 路径。**

### 四步执行结果

| 步骤 | 命令 | 结果 |
|---|---|---|
| check | `check chat.svml` | 退出码 0，`Source is valid`，Outputs 4 |
| plan | `plan chat.svrun` | 退出码 0，`Build plan is valid`，Targets `final.video`，Requests 3（**Local requests 3**），Preflight ready |
| build | `build chat.svrun --follow` | `Build complete`，Build ID `bld_20260923T100423557Z_A07A70298A`，约 6 秒完成，阶段为 `0/3 → 1/3 starting browsers → complete` |
| get | `get <bid> --output final.video --to /tmp/m0/final.mp4` | 退出码 0，`Exported final.video` |

plan 输出确认三个请求全部标注 `local, no Provider charge`，分别是 `@hypit/media-pipeline#render-timeline-audio`、`@hypit/media-pipeline#mux-program-media`、`@hypit/render-hyperframes#render-visual`。

### 成片验证（结构验收）

`/tmp/m0/final.mp4`，77622 字节。

`ffprobe -v error -of json` 实测：

| 属性 | 值 |
|---|---|
| 容器 | mov,mp4,m4a,3gp,3g2,mj2 |
| 时长 | 8.000000 s |
| 视频流 | h264，540×960，30/1 fps |
| 音频流 | aac，48000 Hz，2 声道 |

完整解码测试 `ffmpeg -v error -i final.mp4 -f null -`：**无错误输出，退出码 0**，即全片可解码，不只是头部可读。

这些数值与 Studio snapshot 的 `space` 字段完全一致（canvasWidth 540 / canvasHeight 960 / frameRate 30:1 / frameCount 240 / durationSec 8），说明 snapshot 的画幅时长信息可以作为导出预期的依据。

按 PROJECT_PLAN §9 的要求区分：**以上是结构验收。** 画面内容是否构图合理、文字是否可读，另见下一节的 Studio 截图人工检查，未引入 OCR。

注意：540×960 是该示例模板自身声明的竖屏画幅，**不是 Hypit 默认值**。PROJECT_PLAN §9 建议的 1280×720 需由本项目自己的 title-card 模板显式声明。

## 4. Studio 接口契约（T004）

Studio 启动：`./hypit studio --run ... --workspace ... --runtime ... --port 5599`，输出中打印 Project / Run / Runtime Profile / 浏览器 URL。**端口由 `--port` 显式指定即可确定，本轮未依赖任何默认端口猜测。**

### 契约来源

路由与门禁规则从固定版本源码直接读取，非猜测：
- 路由表：`packages/studio/src/server.ts`
- 快照类型：`packages/studio/src/shared.ts` 第 255 行 `StudioSnapshot`
- 变更类型：同文件第 295 行 `StudioMutation`
- 跨源门禁：`packages/studio/src/mutation-origin.ts`

### 实测端点行为

`GET /__studio/session` → HTTP 200，28440 字节。

顶层键实测为 `['revision','source','run','space','tracks','preview','provenance']`，**不含 `data` 包装**。这与 PROJECT_PLAN §8 的警告一致：响应体本身就是 Snapshot。Qt 侧解析不得预期 `{data:...}`。

`revision: 1`；`source.files` 为 `chat.svrun:run`、`chat.svml:author`、`chat.svs:dependency` 三个文件，即 Run + Author 闭包，非目录扫描。

### 变更（mutation）行为矩阵

同一组断言用 curl 和 Qt 原生 `QNetworkAccessManager` 各跑一遍，结果一致：

| 场景 | HTTP | 响应体 |
|---|---|---|
| 畸形 body（Qt 默认头，无 Origin） | 400 | `Expected a Studio author mutation.` |
| 跨源（`Origin: http://evil.example`） | 403 | `Cross-origin Studio mutations are prohibited.` |
| 过期 revision | 409 | `The Source changed outside Studio.` |
| 当前 revision + 不存在的 entity | 500 | `Studio entity <id> no longer exists.` |

**安全相关结论：** `mutation-origin.ts` 的门禁要求 Host 为 localhost/127.0.0.1/[::1]，Origin 若存在则必须同源，`sec-fetch-site` 若存在必须是 same-origin 或 none。Qt 的 `QNetworkAccessManager` 默认不发 Origin 也不发 sec-fetch-site，因此**能够通过该门禁**（400 而非 403 即证明请求已进入 body 校验阶段）。这是 Qt 原生写入路径可行的前提条件，已实测确认。

### 未能验证的项

**本示例的 snapshot 暴露 0 个可写属性字段。** 唯一 clip 的 `inspector` 数组为空，三个 `editHandles`（move / trim-start / trim-end）全部 `enabled: false`，`disabledReason` 为"该时间表达未开放时间轴回写"。

因此 **T004 要求的"一个实际可写属性的 mutation 成功路径"未验证**。按 CODEX_START 的限制"不要为制造可写属性而编造字段"，本轮不伪造。

已定位可写字段的真实来源：组件需在其 Studio facet 中声明 `{ name, writable: true }` 的 binding。仓库内已有此类声明的包括 `packages/ranking-studio`、`packages/media-track-studio`、`packages/performance-studio` 以及 `examples/complex-explainer/packages/*/src/studio.js`。其中 `complex-explainer` 同样是纯本地端点，但其 README 说明需另外下载约 455 MiB 媒体归档才能打开，本轮未下载，故未用于验证。

这条路径是 **M1/M2 的首要任务**：要么下载 complex-explainer 媒体归档取得可写字段样本，要么在自建 title-card 模板中声明 `writable: true` binding 后再验证成功写入。在此之前不得声称原生属性编辑可用。

## 5. Qt 嵌入与媒体兼容（T005）

三个探针位于 `probes/qt-webengine/`，用 CMake + Qt 6.11.2 真实编译（`cmake --build` 全部 `Built target`，无警告级失败），退出码即断言结果。

### T005a Studio 嵌入 —— PASS

`./build/studio_probe http://localhost:5599/ /tmp/m0/studio_shot.png 45000` → **退出码 0**。

`loadFinished ok=true`，抓帧 2560×1730（Retina 缩放后的实际像素），空白检测 `blank=false`，PNG 已保存（261315 字节）。

人工检查 `/tmp/m0/studio_shot.png` 内容：确认是可用的 Studio 编辑器界面，非错误页。可见区域包括顶栏（hypit logo、chat.svml、工作台/评论）、左侧源码面板（源文件/任务/产物页签）、中部竖屏预览播放器（播放控件与进度条）、右侧属性面板（项目/画布/时间线，显示 540×960、9:16、8.00s、30fps）、底部时间线（conversation 轨道，8.00s 片段）。无错误文本。

附带观察：控制台出现两条 iframe sandbox 警告（`allow-scripts` 与 `allow-same-origin` 并存）和一条 macOS 输入法 mach port 噪音，均不影响渲染，不作为失败。

### T005b 媒体解码 —— PASS

`./build/media_probe /tmp/m0/final.mp4 30000` → **退出码 0**。

QWebEngineView 内嵌 Chromium 的实测报告：

```json
{"state":"playing","detail":"","meta":"540x960 dur=8",
 "canPlayMp4":"maybe","canPlayH264":"probably"}
```

`state: playing` 表示 `timeupdate` 事件中 `currentTime` 已超过 0.2 秒，即**真实解码并推进了播放**，不是仅加载成功。`canPlayType('video/mp4; codecs="avc1.42E01E, mp4a.40.2"')` 返回 `probably`，即 H.264 + AAC 组合受支持。

**结论：Hypit 本地 Runtime 导出的 H.264/AAC MP4 可以在本机 Qt WebEngine 中播放。** 这打通了 PROJECT_PLAN §4.4 要求的"预览可解码"实测，该节明确指出不能只验证网页能加载。

### T005c Qt 原生 HTTP —— PASS

`./build/http_probe http://localhost:5599` → **退出码 0，ALL PASS**，5 项断言全部命中预期状态码（详见第 4 节矩阵）。

未验证：其他机器/显卡的 WebEngine 解码能力；H.265、VP9、AV1 等其他编码；音轨输出到实际音频设备（探针为 muted 播放）。

## 6. G0 门禁判定

PROJECT_PLAN §7 定义 G0 为"至少一份真实可解码 MP4、Qt 可显示预览；接口能力清楚"。

| G0 子项 | 判定 | 依据 |
|---|---|---|
| 真实可解码 MP4 | PASS | `bld_20260923T100423557Z_A07A70298A` 导出 8s h264/aac 540×960，`ffmpeg -f null` 全片解码退出码 0 |
| Qt 可显示预览 | PASS | studio_probe 退出码 0，非空白抓帧，人工确认为真实 Studio UI |
| 接口能力清楚 | PARTIAL | 读取路径、快照结构、四类失败语义均已实测；**可写属性的成功写入路径未验证** |

**G0 = PARTIAL。**

不判 PASS 的唯一原因：没有任何一次真实成功的属性写入。读与失败路径已经确凿，但 M3"Qt 修改真实画面"依赖的成功写入仍是空白。按 CODEX_START 的限制，不将未验证的集成标为通过。

## 7. 阻塞项与下一轮任务

### 阻塞项

1. **无可写属性样本（高，挡 M3）。** chat 示例暴露 0 个可写字段。需取得含 `writable: true` binding 的真实 Run。
2. **pnpm 缺失（中，挡 M2）。** 无法 `pnpm --filter <pkg> build`，因此无法编译自建 Author Package。上游锁定 `pnpm@10.33.0`。
3. **计划引用的文件原本不存在（已在本轮补齐）。** `AGENTS.md`、`STATUS.md`、`tasks.json`、`docs/API_CONTRACT.md`、`docs/UPSTREAM_EVIDENCE.md` 在本轮开始前均不存在，CODEX_START 要求的"先读取"无法照做。

### 建议下一轮

先解 1 和 2，再动 UI：

- **T006**：安装 `pnpm@10.33.0`，验证 `pnpm --filter @example/chat-scene build` 可跑通。
- **T007**：取得可写属性样本。优先在最小 Author Package（`examples/minimal-author-package`）基础上声明一个 `writable: true` 的文本 binding，编译后用 http_probe 验证一次真实成功的 `parameter.adjust`（预期 HTTP 200 + revision 递增）。此项完成即可将 G0 提升为 PASS。
- **T008 起**：方可进入 M1 桌面壳。

不建议在 T007 之前开工 Qt 主窗口：属性面板的数据契约取决于真实可写字段的形状，提前写会返工。
