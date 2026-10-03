# Qt × Agent 视频创作工作台实验报告

本文件说明报告内容、证据边界和生成方式。正式交付为可编辑 Word 与离线 HTML，默认写入忽略目录 `.workbench/deliverables-m13/`。姓名、学号、课程保留空白供提交人填写。报告生成成功不等同于整个项目测试集通过。

## 实验目的

验证 Qt 素材操作与真实 Agent 规划能够贯通同一条审阅、批准、执行、预览和本地导出链。真实演示由 `tests/integration/AgentWorkbenchDemo.cpp` 驱动生产窗口控件与信号，使用当前 Codex CLI 登录及真实 Hypit；窗口录像采集本应用 MainWindow，保留实际帧间隔和等待时间，不声称是人工操作录像。

## 系统架构

```text
MainWindow / AgentPanel
  → AgentWorkbenchBridge
  → AgentController → ContextBuilder / ModelClient → Codex JSON 提案
  → PlanReview / ApprovalManager → ToolDispatcher / EditorController
  → Hypit Studio → 版本确认后的 Qt 预览
  → ExportController → plan → build → get → ffprobe → 全片解码
```

原创部分是 Qt 界面、工程/素材管理、原创模板、编辑和版本约束、Agent 上下文/提案/批准流程与导出验收。Hypit 的语言、编译、Studio 和渲染能力，以及 Qt、Codex CLI、FFmpeg 均为复用。官方示例视频的归属不随报告生成而改变。

固定环境版本及上游提交见 `config/version-lock.json`。报告将它们标注为项目固定版本记录；本次交付重新读取的 FFmpeg/ffprobe 版本和视频参数另外存入 `source-manifest.json`。

## Qt 与 Agent 联动实现

- `app/ui/MainWindow.cpp`：选择可见且兼容的已登记素材，准备“交给 Agent 调整”目标；多选、不兼容和任务忙碌等状态不能交接。
- `app/ui/AgentPanel.cpp`：素材交接准备新目标并限定当前组件；生成、审阅与批准分别触发。
- `app/workflow/AgentWorkbenchBridge.cpp`：主程序和真实演示共用的信号/槽连接，连接目标、范围、选中组件、生成、批准与状态反馈。
- `app/agent/ContextBuilder.cpp`：传入实际工程、版本、entityId/fieldId、可写属性和素材绑定。当前 `imagePixelsProvided=false`，没有像素输入，不宣称模型看过画面。
- `app/agent/ModelClient.cpp`：使用当前 Codex 登录取得结构化 JSON 提案。
- `app/agent/ApprovalManager.h`、`ToolDispatcher.cpp` 与 `app/controllers/EditorController.cpp`：审阅、批准、范围和版本校验后执行实际编辑。
- `app/controllers/ExportController.cpp`、`app/services/MediaValidation.cpp`：冻结输入并分层验证计划、Build、Output 和可解码成片；付费渲染 Provider 不通过计划门禁。

## 操作步骤

1. 按 `USER_GUIDE.md` 准备固定版本本地依赖和当前 Codex 登录，不修改全局环境。
2. 准备 `.workbench/showcase/catalog.json` 及 `preparation-manifest.json`；生成脚本见 `scripts/showcase/prepare_hypit_samples.py`。仅下载固定版本 README 明确引用的官方素材，应用启动不联网下载。
3. 运行真实演示：查看示例库，从本地对话样例创建 video-story 工程并播放；导入官方排行榜素材，交给 Agent 生成替换提案；审阅后批准；再请求标题修改，审阅后批准；导出并验证。
4. 确认真演示写出 `docs/evidence/m13/agent-workbench-demo.json` 且 `verdict=PASS`，再生成报告。

```bash
build/tests/agent_workbench_demo
python3 scripts/showcase/build_experiment_report.py \
  --delivery .workbench/deliverables-m13
```

生成器只读取证据和媒体，不会发起模型请求或渲染。依赖本地 Python 3、`python-docx`、FFmpeg 和 ffprobe；可用 `--ffprobe`、`--ffmpeg` 指定路径，用 `--demo`、`--catalog`、`--preparation` 指定证据。仅接受真实演示 PASS、两次模型请求、两次显式批准、批准前源码不变和录像/成片解码通过的证据。

## 测试结果

数值全部来自最终演示 JSON 与对复制后视频的重新验证，不在本源文档中预填通过次数、时长或 Build ID。生成器检查视频复制前后 SHA-256，核对官方与本地素材来源哈希，重新运行 ffprobe，并校验 Word ZIP、Word/HTML 本地链接和章节锚点。四个官方素材使用预先记录的 SHA-256 绑定其已有全片解码证据；最终演示 JSON 没有记录视频 SHA-256，仅有解码成功布尔值，因此对操作录像、工程成片交付副本重新完整解码。本地对话样例也由生成器补充全片解码。

完整测试集使用仓库独立证据。本次演示没有故障注入，报告只说明模型失败、过期方案、409 冲突、422 回滚及导出失败的既有处理边界，不声称录像逐一验证这些情况。若演示未通过或来源不一致，生成器失败退出，不生成新的通过报告。

首轮采集曾因 Retina 图像设备像素比保留而使窗口画面只占编码画布的一部分。采集器归一化缩放图像设备像素比后重录完整真实演示；交付以修正后最终演示证据为准。最终 JSON 的请求次数仅属于该次演示，不等于全部开发与重录期间的请求总量。

## 演示视频

交付目录包含：

| 文件 | 用途 |
| --- | --- |
| `index.html` | 离线报告入口，内嵌相对路径视频控件 |
| `experiment-report.docx` | 可编辑中文 Word 报告，含相对视频链接 |
| `videos/qt-agent-walkthrough.mp4` | 完整真实窗口录制，保留模型等待 |
| `videos/qt-agent-film.mp4` | 本次工程实际导出成片 |
| `videos/official-01.mp4` 至 `official-04.mp4` | 四个不同的官方示例，顺序以来源清单为准 |
| `videos/local-chat.mp4` | 上游本地对话样例 |
| `images/stage-*.jpg` | 若原录制帧存在，复制对应阶段真实窗口帧 |
| `evidence/` | 演示 JSON、素材目录/来源、版本锁和本说明 |
| `source-manifest.json` | 来源、官方 URL/压缩包成员、哈希、参数和重新验证结果 |

一起移动整个交付目录即可保留视频链接。官方来源在固定版本 `examples/complex-explainer/README.md` 与素材准备清单中记录。既有三个 MP4 若哈希相同只计一个不同视频；7 秒访谈片段不足当前 8 秒模板时长，仅供示例预览。本轮成片使用原创 video-story 模板和官方排行榜素材，固定 8 秒且静音。

## 总结

实验关注可追溯的 Qt → Agent → Hypit 实际联动：目标准备不会写工程，批准前保留源码，批准后等待版本与预览一致，导出最终以可解码文件为交付条件。局限包括当前 Codex 登录依赖、本轮两项受限编辑、没有视频视觉理解证据，以及 8 秒静音模板边界。报告附真实录制、操作阶段和机器证据，不以截图代替功能断言。
