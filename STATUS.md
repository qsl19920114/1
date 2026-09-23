# STATUS.md

**最后更新：2026-09-23**

进度以本仓库状态与 `docs/evidence/` 下的证据文件为准，不以任何会话的口头声称为准。恢复工作前请先核对证据文件是否真的存在。

## 当前位置

**M0 已执行完毕，G0 判定为 PARTIAL。尚未进入 M1。**

应用代码尚未开始编写：`app/` 目录目前为空骨架。已完成的是风险验证与契约固定，这是 PROJECT_PLAN §7 要求的前置条件（"先做 M0 的真实小闭环，不能在两周 UI 之后才第一次测试视频导出"）。

## G0 三项子门禁

| 子项 | 判定 | 证据 |
|---|---|---|
| 真实可解码 MP4 | PASS | `docs/evidence/m0/ffprobe_final.json`、`build.log` |
| Qt 可显示预览 | PASS | `docs/evidence/m0/studio_shot.png`、`studio_probe.txt` |
| 接口能力清楚 | PARTIAL | `docs/API_CONTRACT.md`、`session.json`、`http_probe.txt` |

**G0 不判 PASS 的唯一原因：没有任何一次真实成功的属性写入。**

读取路径和四类失败语义（400 / 403 / 409 / 500）都已实测确凿，但成功写入（HTTP 200 + revision 递增）这条路径是空白。chat 示例的唯一 clip 的 `inspector` 是空数组，没有可写字段可试。按项目约束不伪造字段来凑证据。

## 已确凿的事实

这些不需要重新验证，直接用：

- Hypit 固定在 **0.2.10**，commit `1af179d3f58284c2d6d3c1f63052172a4fe1b5a6`。CODEX_START.md 提到的 0.2.12 是错的。
- 本机 Qt **6.11.2** 含 WebEngineWidgets，满足计划的 6.8 最低目标。CMake 4.4.3。
- Hypit 本地 Runtime 导出的 **H.264/AAC MP4 能在 Qt WebEngine 中真实解码播放**（不只是加载成功）。
- 真实 Studio 能在 QWebEngineView 中完整渲染（源码面板 / 预览 / 属性 / 时间线俱全）。
- **Qt 原生 `QNetworkAccessManager` 能通过 Studio 的跨源门禁**（返回 400 而非 403 即证明进入了 body 校验阶段）。这是原生写入路径可行的前提。
- 调用任何 hypit 命令**必须同时传 `--workspace` 与 `--runtime`**。
- `GET /__studio/session` 的响应体**就是** Snapshot，无 `{data:...}` 信封。

## 阻塞项

| # | 阻塞项 | 严重度 | 挡住 |
|---|---|---|---|
| 1 | 无可写属性样本 | 高 | M3，且 G0 无法升 PASS |
| 2 | pnpm 未安装（上游锁定 10.33.0） | 中 | M2 自建模板 |

阻塞项 1 的细节：可写字段来自组件 Studio facet 中的 `{ name, writable: true }` 声明。仓库内有此声明的包已定位（`ranking-studio`、`media-track-studio`、`performance-studio`、`complex-explainer/packages/*` 等），但 `complex-explainer` 需另下载约 455 MiB 媒体归档才能打开，本轮未下载。

## 下一轮做什么

按顺序，不要跳：

1. **T006** 安装 pnpm@10.33.0，验证 `pnpm --filter @example/chat-scene build` 退出码 0。
2. **T007** 取得可写属性样本，验证一次真实成功的 `parameter.adjust`。完成即可把 G0 升为 PASS。
3. **T008** 之后才动 Qt 主窗口。

**不建议在 T007 之前开工 Qt 属性面板。** 属性控件的数据契约取决于真实可写字段的形状（`inspector[].edit` 里到底有什么），提前写会返工。

## 环境复现

```bash
# 构建并运行 M0 探针
cd probes/qt-webengine
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j4

# 启动 Studio（在 hypit 检出目录中）
cd ../../../hypit
./hypit studio --run examples/semantic-composition/chat.svrun \
  --workspace examples/semantic-composition \
  --runtime examples/semantic-composition/hypit.runtime.json \
  --port 5599

# 回到本仓库跑探针
cd -
./build/http_probe http://localhost:5599          # 期望 ALL PASS，退出码 0
./build/studio_probe http://localhost:5599/ shot.png 45000   # 期望退出码 0
./build/media_probe <path-to-exported.mp4> 30000            # 期望 state=playing
```
