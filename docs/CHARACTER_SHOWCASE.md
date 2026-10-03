# Qt 内嵌 AI · 人物素材创作演示

## 本轮要展示什么

用户确认采用“替换人物图片或视频素材，保留版式和动画”。在 FrameLab 中选中已登记的人物素材，点击 **AI 替换人物素材**，再生成方案、审阅并确认。正式 AgentWorkbenchBridge 把 Qt 控件连接到真实 Codex 模型与编辑控制器；Hypit 负责真实编译、预览及本地导出。

此操作替换整个图片/视频素材绑定。它不会只改动原视频中的一张脸，也不重新生成原视频动作。固定版本 Hypit 的 `examples/interview/swap-host.svml` 使用 `gpt:Image` 和 `seedance:ReferenceVideo`，对应 runtime 有远程 `hypihub.default`。本次只复用公开归档中的人物图片并调用本地模板渲染；没有配置或调用这些生成 Provider。

## 产品结构

- `MainWindow::prepareSelectedAssetGoal` 共用单选、可见性、兼容字段与忙碌状态检查。人物入口仅准备目标，不自动生成或修改。
- `AgentPanel::composeCharacterGoal` 把素材名称和精确路径编码为 JSON，限定当前组件，要求保留其他属性。新目标撤销旧批准。界面显示简短组件名，完整 ID 保留在提示中。
- 真实模型输出经过既有方案、可写字段、工程版本和本地批准检查。确认执行后等待编译快照与可视预览一致。
- `MediaPlayerWidget` 拥有媒体页面、播放状态及 Qt 控件，可复用于成片、示例和素材预览。`MediaPlayerDialog` 管理窗口、全屏与退出；中央工程预览仍由 StudioTransport 管理。
- 播放器通过实际 HTML video 状态更新界面，支持播放/暂停、停止归零、前后5秒、进度拖动、0.5/1/1.5/2倍速、音量、静音、循环、全屏及 Esc 退出。加载失败显示实际错误；关闭释放页面与独立 profile。

## 实验步骤

1. `python3 scripts/showcase/prepare_character_materials.py`：从已下载官方归档中提取三份明确选择的 PNG。保留原始字节、归属、来源 URL、归档成员、哈希和解码结果。上游仓库不修改。
2. 编译后运行显式目标 `build/tests/character_showcase_demo`。该目标使用当前 Codex 登录；它不加入常规 CTest，避免自动回归触发模型请求。
3. 真实 Qt 工程导入三张人物图，导出基准人物。用正式 AI 入口分别替换两次，每次核对批准前源码未改、批准后仅素材绑定变化，并导出成片。
4. 用新版 Qt 播放器打开最终成片，操作播放、定位、倍速、音量、静音、循环、全屏/退出和停止。
5. `python3 scripts/showcase/build_character_showcase.py`：仅在真实演示 PASS 且交付文件哈希匹配时制作本地视频展示页。

## 交付

`.workbench/deliverables-m14/index.html` 集中展示真实操作录像和三份8秒成片。`videos/` 中存放复制后可播放文件；`evidence/` 保存执行与素材来源；`manifest.json` 记录哈希和参数。`runs/` 保留原始实际工程与连续采集帧，不需复制全部原始帧即可播放展示页。

操作录像由测试脚本驱动生产控件/信号，连续采集实际本应用窗口，保留模型等待和导出时间；不声称是人工操作视频。所有人物图片在本地使用，模型没有接收图片像素。三份素材复用 Hypit 官方归档，白帽主持人与虚构插画有明确生成归属；黑帽人物的单独生成历史未独立验证，来源记录保留这一差别。

测试和发布以 `docs/evidence/m14/` 的实际记录为准，未执行项目不计入通过。保留1.5的报告和视频，不覆盖旧交付。
