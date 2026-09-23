# 基于 Qt 的可编辑视频模板与 Agent 创作工作台

项目代号 Qt Video Workbench。Qt 交互软件课程大作业。

**当前状态：M0 风险验证已完成，G0 = PARTIAL。应用代码尚未开始编写。** 详见 `STATUS.md`。

## 这是什么

Qt 6 桌面应用：新建视频工程、选模板、导入本地素材、用原生属性控件改标题/颜色/图片、看预览、保存可继续编辑的工程、导出真正的 MP4 并校验成片。增强版再加自然语言修改（Agent 给出可核对的编辑提案，用户确认后才落地）。

本项目自己实现工程与素材管理、模板实例化、原生属性编辑、版本安全、异步任务管理与交付验收。视频语言、编译和渲染复用 [Hypit](https://github.com/hypit-ai/hypit)；浏览器预览先复用其 Studio。

原创部分与复用部分的边界在 `PROJECT_PLAN.md` §6 与 §10 有明确划分。

## 仓库里有什么

```
app/                 Qt 应用源码（骨架，尚未实现）
  ui/ controllers/ domain/ backend/ services/ infrastructure/ resources/
templates/           原创模板与 manifest（尚未实现）
probes/qt-webengine/ M0 验证探针，已编译可运行
config/              version-lock.json：M0 实测固定的版本组合
docs/                环境报告、接口契约、证据文件
tests/               unit/ contract/ integration/ e2e/ fixtures/
```

Hypit **不** vendor 进本仓库，通过 `config/version-lock.json` 的 `distributionPath` 指向本机检出（默认 `../hypit`）。

## 已经验证过的事实

M0 的五项探测都有真实证据，留档在 `docs/evidence/m0/`：

- Hypit 固定在 **0.2.10** / commit `1af179d3`（CODEX_START.md 里的 0.2.12 与实际不符）
- 官方 chat 示例完成真实 check → plan → build → get，导出 8 秒 **h264/aac 540×960 30fps** MP4，`ffmpeg -f null` 全片解码无错
- 该 MP4 在 **Qt WebEngine 中真实解码播放**（`state=playing`，不只是加载成功）
- 真实 Studio 在 QWebEngineView 中完整渲染（源码面板 / 预览 / 属性 / 时间线）
- Studio 的四类失败语义实测：畸形 400 / 跨源 403 / 过期 revision 409 / 未知 entity 500
- **Qt 原生 `QNetworkAccessManager` 能通过 Studio 跨源门禁**，原生写入路径可行

接口契约全文见 `docs/API_CONTRACT.md`，每条断言都标注了源码行号或实测状态码。

## 还没验证的

**没有任何一次真实成功的属性写入。** chat 示例的 clip `inspector` 是空数组，没有可写字段可试；按项目约束不伪造字段凑证据。这是 G0 判 PARTIAL 的唯一原因，也是下一轮的首要任务。

另外 pnpm 未安装，暂时无法重新编译 Author Package，挡住 M2 自建模板。

## 环境要求

| 依赖 | 实测版本 |
|---|---|
| macOS | 15.7.7 arm64 |
| Qt | 6.11.2（需 WebEngineWidgets） |
| CMake | 4.4.3（最低 3.21） |
| 编译器 | Apple clang 17，C++17 |
| Node | 22.22.1（Hypit 要求 >=22.15） |
| FFmpeg / ffprobe | 9.0.2 |
| pnpm | 10.33.0（**未安装**） |

## 跑 M0 探针

```bash
cd probes/qt-webengine
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j4
```

三个探针，退出码即断言结果：

- `http_probe <base-url>` — Studio 接口契约，期望 `ALL PASS`
- `studio_probe <url> <out.png> [timeoutMs]` — Studio 嵌入渲染，期望非空白抓帧
- `media_probe <video-file> [timeoutMs]` — WebEngine 解码，期望 `state=playing`

启动 Studio 时注意**必须同时传 `--workspace` 与 `--runtime`**：

```bash
cd ../hypit
./hypit studio --run examples/semantic-composition/chat.svrun \
  --workspace examples/semantic-composition \
  --runtime examples/semantic-composition/hypit.runtime.json \
  --port 5599
```

## 文档索引

| 文件 | 内容 |
|---|---|
| `STATUS.md` | 当前进度、阻塞项、下一轮任务 |
| `PROJECT_PLAN.md` | 完整执行计划、产品边界、门禁定义 |
| `AGENTS.md` | 给开发工具的稳定约束 |
| `tasks.json` | 任务状态与门禁判定 |
| `docs/API_CONTRACT.md` | Studio 接口契约（带源码行号与实测状态码） |
| `docs/ENVIRONMENT_REPORT.md` | M0 环境报告与 G0 判定依据 |
| `docs/evidence/m0/` | 原始证据：session 快照、探针输出、截图、ffprobe 结果 |

## 第三方依赖说明

Hypit 的 LICENSE 带附加条件，使用时需按实际版本保留并复核，不等同于标准 Apache 2.0。详见 `PROJECT_PLAN.md` §10。
