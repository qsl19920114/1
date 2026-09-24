# STATUS.md

**最后更新：2026-09-24**

进度以本仓库状态与 `docs/evidence/` 下的证据文件为准，不以任何会话的口头声称为准。恢复工作前请先核对证据文件是否真的存在。

## 当前位置

**G0 = PASS。M0 全部完成，T006、T007、T008 已完成。下一步是 T009（设置与日志）。**

Qt 应用已有可运行的四区域骨架（T008），但**尚未连接真实 Studio 进程**——目前只能从离线 JSON 文件载入会话。自动启动 Studio 是 T010、接真实会话是 T011。

M0 先做风险验证与契约固定，是 PROJECT_PLAN §7 的要求："先做 M0 的真实小闭环，不能在两周 UI 之后才第一次测试视频导出"。

## G0 三项子门禁

| 子项 | 判定 | 证据 |
|---|---|---|
| 真实可解码 MP4 | PASS | `docs/evidence/m0/ffprobe_final.json`、`build.log` |
| Qt 可显示预览 | PASS | `docs/evidence/m0/studio_shot.png`、`studio_probe.txt` |
| 接口能力清楚 | PASS | `docs/API_CONTRACT.md`、`docs/evidence/m1/http_probe_write.txt` |

第三项在 M0 结束时是 PARTIAL，缺口是"没有任何一次真实成功的属性写入"。T007 已补上。

## T007：写入路径打通的完整链条

上游 chat 示例的 `inspector` 是空数组。根因是该组件**不带 Studio Companion**——缺的只是声明层，不是能力缺失。

补一个 Companion facet 后（Surface 解码器、manifest、渲染器全不动），同一个 clip 的 `inspector` 从 0 个字段变成 2 个，都带 `edit`。随后 Qt 原生代码完成了真实写入：

| 环节 | 实测结果 |
|---|---|
| HTTP 状态 | **200**，body `{"revision":8}` |
| revision 推进 | 7 → 8 |
| SVML 源文件 | 真被改写为 `title="Qt 写入验证"` |
| 重编译后的快照 | 反映新值 |
| 重新 build | 成功，`bld_20260924T023239809Z_59D03451EF` |
| 导出 MP4 | ffprobe 通过，全片解码无错 |
| 抽帧内容验收 | **画面标题真的变成了写入的中文文本** |

最后一行是关键。按 AGENTS.md 第 3 条，HTTP 200 只是第一层；只有抽帧确认画面变化，才算真的写进去了。

fixture 与复现步骤在 `tests/fixtures/writable-probe/`。

## 已确凿的事实

这些不需要重新验证，直接用：

- Hypit 固定在 **0.2.10**，commit `1af179d3f58284c2d6d3c1f63052172a4fe1b5a6`。CODEX_START.md 提到的 0.2.12 是错的。
- 本机 Qt **6.11.2** 含 WebEngineWidgets，满足计划的 6.8 最低目标。CMake 4.4.3。pnpm **10.33.0** 已装，可编译 Author Package。
- Hypit 本地 Runtime 导出的 **H.264/AAC MP4 能在 Qt WebEngine 中真实解码播放**（不只是加载成功）。
- 真实 Studio 能在 QWebEngineView 中完整渲染（源码面板 / 预览 / 属性 / 时间线俱全）。
- **Qt 原生 `QNetworkAccessManager` 能通过 Studio 的跨源门禁**（返回 400 而非 403 即证明进入了 body 校验阶段）。这是原生写入路径可行的前提。
- 调用任何 hypit 命令**必须同时传 `--workspace` 与 `--runtime`**。
- `GET /__studio/session` 的响应体**就是** Snapshot，无 `{data:...}` 信封。

### T007 新发现的三条约束

这三条直接影响 Qt 实现，别踩：

1. **字段可写的判定是 `inspector[].edit` 是否存在，不是 `control`。** 源码依据：`parameters.ts:417` 要求 `declaration.writable === true` 且属性非引用；`:495` 仅此时才输出 `edit` 键。
2. **不能假设 `edit.source.range` 非空。** 被省略的属性给出空区间（实测 `{650,650}`），写入是向源文件**插入**而非替换。
3. **端口以 stdout 实际打印为准。** 被占用时 Studio 自动递增（实测 5610→5611）。另外改动包代码或 activation 后**必须重启 Studio**，它不热加载包模块。

## T008：Qt 四区域骨架

构建与验收证据在 `docs/evidence/m1-t008/`：

| 验收项 | 结果 |
|---|---|
| 全新构建（清空 build 后） | `BUILD_EXIT=0` |
| 四区域布局 | 抽帧人工确认：工程与组件 / 预览 / 属性 / 任务与日志 |
| 属性表按契约渲染 | 标题=文本=Launch crew=可编辑是；入场帧数=数值=10=可编辑是 |
| 状态栏 | `revision 1 · 可写字段 2 个` |
| 数据真的进了界面 | 空载与载入截图哈希不同（20c17848 vs 641546b6）|

为避开桌面辅助权限依赖，主窗口支持 `--selftest --out=<png> [--session=<json>]`，用 `widget->grab()` 自绘截图后退出，可进 CI。退出码：0 成功 / 2 缺 `--out` / 3 会话读取或映射失败。

**已知限制：`--session` 必须用等号形式。** 空格形式（`--out a.png --session b.json`）会被 Qt 静默丢弃并截出空图返回 0。尝试三次拦截均失败：`isSet()` 为 false、`positionalArguments()` 为空、`app.arguments()` 已被消化，解析后无痕迹可查。已把 `--selftest` 改为布尔标志缓解，残留风险记入 `tasks.json` 的 `knownIssue`。

## 阻塞项

无。M0 遗留的两项均已解除：

| 原阻塞项 | 解除方式 |
|---|---|
| 无可写属性样本 | T007：补 Studio Companion facet，fixture 在 `tests/fixtures/writable-probe/` |
| pnpm 未安装 | T006：`npm install -g pnpm@10.33.0`，两个 build 均退出码 0 |

## 下一轮做什么

按顺序做，T009 → T010 → T011，最后一步冲 G1。

**T009 设置与日志。** 从 `config/version-lock.json` 读 `distributionPath`，启动时自检 hypit 可执行；缺失或版本不符给可读错误而不是崩溃；日志写文件并包含命令行与退出码。

**T010 自动启动 Studio。** 用 `QProcess` 拉起 `hypit studio`，**必须同时传 `--workspace` 与 `--runtime`**；从 stdout 解析 Local URL 行作为就绪信号，不假定 `--port` 生效（端口被占时上游会自动递增）；关窗时优雅终止子进程，不留孤儿进程。

**T011 接真实会话（G1）。** 把现在的离线 JSON 载入换成 `GET /__studio/session`。属性面板的数据契约已确定：

- 控件可用性看 `edit` 是否存在，**不看** `control`；
- 控件类型按 `control` 映射（`text` / `number` / `boolean` / `select` / `color`，另有 `list` / `record` 首版只读）；
- 不可写字段显示服务端的 `disabledReason` 原文，两种文案分别对应"引用绑定"与"组件声明只读"。

G1 的门禁是"从 Qt 打开工程并读取真实会话，关闭不乱杀进程"。

## 环境复现

### 构建并运行 Qt 应用

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j4

./build/app/qt-video-workbench                      # 直接启动

# 无人值守验收（注意 --session 必须用等号形式）
./build/app/qt-video-workbench --selftest --out=/tmp/shell.png \
  --session=docs/evidence/m1/session_with_writable_fields.json
```

### 构建并运行 M0 探针

```bash
cd probes/qt-webengine
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j4

# 启动 Studio（在 hypit 检出目录中）
cd ../../../hypit
./hypit studio --run examples/semantic-composition/chat.svrun \
  --workspace examples/semantic-composition \
  --runtime examples/semantic-composition/hypit.runtime.json \
  --port 5599

# 回到本仓库跑探针（用 stdout 实际打印的端口）
cd -
./build/http_probe http://localhost:<port>                     # 契约断言，期望 ALL PASS
./build/studio_probe http://localhost:<port>/ shot.png 45000    # 嵌入渲染，期望退出码 0
./build/media_probe <exported.mp4> 30000                        # 解码，期望 state=playing
```

验证成功写入路径需要一个带可写字段的 Run，搭建步骤见 `tests/fixtures/writable-probe/README.md`，然后：

```bash
./build/http_probe http://localhost:<port> --write
```
