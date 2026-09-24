# 当前状态

最后更新：2026-09-24。以仓库代码、tasks.json 和 docs/evidence/ 的实际证据为准。

**G0 = PASS；G1 = PASS。T001–T011 已完成。下一步是 M2 的工程持久化、素材管理与原创模板。**

## 当前应用能做什么

- 读取版本锁，按仓库根目录解析 Hypit distributionPath；异步检查 0.2.10 版本，错误会显示在窗口及日志。
- 通过“打开工程”选择 Run、workspace、Runtime；QProcess 同时传入三个绝对路径。
- 从 stdout 的 Local 行识别实际地址，支持端口被占时自动回退，不猜端口。
- QNetworkAccessManager 读取真实 `/__studio/session`，映射组件树与当前选中组件的属性。
- 原生面板映射文本、数值、布尔、选项、颜色；当前均只读，服务端的 disabledReason 保留。
- QWebEngineView 嵌入完整 Studio，显示真实编译预览；网页修改后点击“刷新会话”更新原生面板。
- 关闭工程或窗口时只停止本应用创建的 Studio 和其 Unix 进程组，不终止其他会话或共享 Worker。
- 保留离线 `--session` 和 `--selftest --out` 截图模式。

## 本轮验证结果

证据目录：`docs/evidence/m1-live/`。

| 检查 | 结果 | 证据 |
|---|---|---|
| CMake 编译 | 退出码 0 | build.log |
| QtTest | 5/5 测试套件通过 | ctest.log |
| CLI 参数和错误退出码 | 7/7 通过 | cli-cases.json |
| 真实 Studio 连接 | revision 1、1 条轨道、2 个可写字段 | session.json、e2e.json |
| 本机端口占用 | 自动使用另一端口并连接正确 | e2e.json、live-run.log |
| 编译预览 | iframe 内存在真实 composition；截图人工确认 | live-studio.png |
| 关闭窗口 | 自有进程组已结束，无关监听进程仍在 | e2e.json |
| 中文和空格路径 | 使用 `.workbench/真实 工程-*` 验证 | live-run.log |

`tests/integration/live_studio.py` 可重新生成整套真实证据。只使用本地 media / hyperframes Provider。测试没有修改 Hypit 检出。

## 本轮修复的已有缺口

- 未提交的 T009 代码把 `../hypit` 错算为 `<repo>/hypit`，已修复并添加回归。
- 启动自检由阻塞等待改为异步进程，结构化日志使用 JSONL（参数数组与退出码独立存储）。
- 旧 T008 所记的 `--session` 丢失，实际是 QApplication 吞掉同名会话选项。现在创建 QApplication 前保存 argv，两种参数形式均通过测试。
- 审查发现取消回调内仍可能发布旧快照；通过请求代次检查修复，并有回归测试。
- 明确先释放 WebEngine 页面再释放 Profile，真实关闭日志中已无原生命周期警告。

## 原有 G0 证据

- 本地示例真实 MP4 导出、ffprobe 和全片解码：`docs/evidence/m0/`。
- 原生 HTTP 成功参数写入、源码变化、重新编译导出和抽帧确认：`docs/evidence/m1/`。
- Hypit 固定为 0.2.10 / commit `1af179d3f58284c2d6d3c1f63052172a4fe1b5a6`。
- 接口依据仍为 `docs/API_CONTRACT.md`；G0 的写入探针通过不代表应用已实现原生编辑。

## 尚未完成

- M2：应用自己的工程存储、不可变素材导入、原创 title-card 模板。
- M3：Qt 原生版本安全写回、撤销/重做和外部冲突处理。
- M4：应用内 build/status/cancel/get 编排及导出验收。
- 后续 Agent 增强和独立分发包。

验证平台是 macOS 15.7.7 / Qt 6.11.2；Windows 子进程树管理和跨平台打包尚未验收。当前只接受已验证的本地媒体与 Hyperframes Runtime Provider。
