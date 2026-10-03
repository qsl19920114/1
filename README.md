# FrameLab · 灵感片场

深色 Qt 视频创作工作台。通过当前 Codex 登录理解创作目标，审阅方案后创建本地图片故事工程，再局部修改和导出。保留视频示例、原生编辑与播放定位。使用方法见[使用指南](docs/USER_GUIDE.md)。

Qt 6 Widgets 视频工程应用。版本 **1.2.0**：真实模型、三段创作方案审阅、多步骤执行与任务恢复，共用原有工程、素材、编辑历史和经过媒体校验的 MP4 导出。进度见 [STATUS.md](STATUS.md)。

## 构建与启动

验证平台：macOS 15.7.7 arm64、Qt 6.11.2（含 WebEngineWidgets/Test）、CMake 4.4.3、Hypit 0.2.10。M2 实测 Node 25.8.2；M0/M1 历史环境为22.22.1。本轮未安装或修改全局环境。

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build -j4
./build/app/qt-video-workbench.app/Contents/MacOS/qt-video-workbench
```

### Agent 创作流程

1. 在右侧「Agent 创作」选择三张本地 PNG/JPEG，输入“做15秒校园招新短片，开场3秒、主体8秒、结尾4秒”。
2. 等待真实 Codex 生成方案。修改三张卡片的文案、图片、字号、颜色、时长和顺序，再确认新工程目录。
3. 工作台创建原创720×1280、30fps工程，逐项写入并回读。生成方案阶段不会修改工程；停止保留已完成步骤。
4. 选择结尾组件，勾选「下一次请求仅当前组件」，输入“标题改为周五一起玩！其他内容保持”。审阅真实字段差异后确认。
5. 可通过共享编辑历史撤销上一步。重开工程后恢复任务需要重新核对版本和批准剩余动作。
6. 点击「导出MP4…」。只有真实 Build、指定 Output、参数检查及全片解码均通过，才交付所选版本。

需要本机可运行的 Codex CLI 和当前登录。若 Finder 启动时 CLI 不在 PATH，在配置的 `tools.codex` 指定可执行文件绝对路径；凭据由 Codex 自己使用，应用不读写凭据。模型仅收到所选图片的名称、尺寸、ID及必要工程字段，不上传图片像素。完整说明见[Agent 架构](docs/AGENT_ARCHITECTURE.md)。

### 手动创作流程

1. 点击“新建工程”，输入名称、选择视频故事或图片标题卡模板及保存位置，创建独立工程。
2. 点击“导入图片”，选择 PNG/JPEG。素材列表显示名称、真实尺寸；原图保持不变。
3. 选择图片素材及组件，点击“应用到选中组件的图片”；也可双击素材复制相对路径。**导入与应用到画面是两个动作。**
4. 在右侧原生属性修改标题、副标题、主题色、图片路径或入场帧数。修改经后端确认后进入历史，使用工具栏“撤销/重做”。
5. “编辑源码…”支持当前会话列出的单个文件；编译失败时核对文件内容后恢复。检测到外部修改会刷新并重置历史，不覆盖外部变化。网页中修改后可点击“刷新会话”。
6. “保存工程”保存应用元数据，不触发导出。关闭后通过“打开工程”选择 `workbench.qvw.json` 继续工作。移动整个工程目录后仍可重开。

7. 点击“导出 MP4…”选择目的地。只有本地计划与 Provider 检查、精确 Output 获取、参数检查和全片解码通过后才交付。任务面板显示阶段、ID和源码版本。

“停止观察”保留后台 Build；“恢复观察”查询同一任务。提交尚未返回ID时，只在独立Runtime中查找唯一Build；无法确定时保留未知记录，不重复提交。“取消构建”针对当前Build并核对终态。冻结输入修改、工程搬迁或Hypit版本不符可能阻止历史任务恢复；工程本身仍可重开。

编辑期间原生控件暂时禁用。日志会区分冲突、编译拒绝和无法确认的网络结果；遇到冲突先核对刷新后的内容，再决定是否重新修改。Studio 文件监听重编译期间也可能短暂返回冲突。

模板为1280×720、30fps、8秒，包含本项目原创的文字/图片布局、简单出现动画和几何占位图。新工程包含自己的源码、Run、Runtime、原创包和素材，不依赖上游示例目录。

也可直接指定工程清单：

```bash
./build/app/qt-video-workbench.app/Contents/MacOS/qt-video-workbench --project="/绝对路径/校园社团介绍/workbench.qvw.json"
```

“打开 Run…”保留高级入口，选择既有 `.svrun`、workspace 和本地 Runtime。命令行对应 `--run`、`--workspace`、`--runtime`；与 `--project`、离线 `--session` 互斥。

## 辅助模拟提案

右侧“编辑提案 · 本地模拟”支持 `标题改为校园摄影社`、`主题色改为#e47735`、`图片使用第1张`。生成后核对当前值、拟修改值及来源，再点击“确认修改”；放弃或过期提案不修改工程。确认写入与原生编辑共用控制器，成功后可撤销。

可导入64KiB以内的单项提案JSON。Schema为 `qvw.edit-proposal@1`，必须包含当前revision、sourceFingerprint（SHA256的64个十六进制字符）、真实entityId/fieldId、简单值及origin。来源由导入入口确定；JSON自称“真实模型”不能提高可信度。未知键、多项操作、Source/Shell、不可写字段、过期版本和工程外图片均拒绝。

此辅助入口仍使用明确标注的本地规则演示。主入口「Agent 创作」使用真实 Codex；两者有各自的方案格式和来源标签，共用版本保护与编辑历史。

## 工程、素材与配置

- 清单仅存元数据与工程内相对路径；源文件仍是编辑事实，不维护第二套时间线。
- 素材按实际内容解码，使用 SHA-256 命名、复制原始字节并去重。限制为4000万像素、64MiB；清单 `settings.maxAssetBytes` 可进一步降低字节上限。
- 创建不会覆盖非空目录；保存使用原子替换；素材登记失败不发布半完成索引。详情见 [架构与格式](docs/ARCHITECTURE.md)。
- `config/version-lock.json` 中 `hypit.distributionPath` 相对仓库根目录解析，默认 `../hypit`。上游依赖仍由外部 Distribution 提供，不复制进工程。
- `--config` 或“选择配置”切换版本锁；`--log` 指定 JSONL 日志路径，默认在 Qt AppDataLocation。
- Runtime 只接受已验证的本地 media/hyperframes Provider。关闭工程会结束应用启动的 Studio 进程组。

模板随应用复制到相对资源目录，macOS 为 `Contents/Resources/templates`，不依赖编译机的绝对模板路径。发布包内默认配置优先；外部 Hypit、Node、FFmpeg/ffprobe 和渲染浏览器仍须准备。详细布局见 [使用指南](docs/USER_GUIDE.md)。

日志每条有界，文件超过5MiB时轮转，保留两份备份。“清理当前终态缓存”先核对精确Build终态、独立Runtime活动与Worker停止，再删除自有UUID冻结目录和当前任务记录；工程输入和成片保留。

## 验证（不截图）

```bash
ctest --test-dir build --output-on-failure
node --test tests/template/title-card.test.mjs
node --test tests/template/story-reel.test.mjs
./build/tests/project_e2e_test
./build/tests/editor_e2e_test
./build/tests/export_e2e_test
./build/tests/proposal_e2e_test
./build/tests/agent_e2e_test
./build/tests/walkthrough_demo
```

各项 E2E 需本机监听端口权限及实际 Hypit 依赖。工程测试覆盖创建、导入、迁移重开；编辑测试覆盖原生写入、数值转换、撤销/重做、真实409/422、源码恢复及重开一致。它们核对编译预览与实际图片响应，不依赖截图。

CTest中的官方启动器契约用例需要同级`../hypit/hypit`固定版本脚本，用于核对真实wrapper；该文件仅复制到临时测试目录，不进入本仓库。其余大多数单测使用本地进程/HTTP fixture。

M4真实导出涵盖输入冻结、停止与同ID恢复、取消和编译失败不交付；实际成片为1280×720、30fps、8秒H.264。见 [M4证据](docs/evidence/m4/README.md)。

结果见 [M3证据](docs/evidence/m3/README.md) 和 [M2证据](docs/evidence/m2/README.md)。旧 `--selftest --out` 和 M1 截图脚本仅保留为兼容工具，不作为当前验收步骤。

## 主平台交付

```bash
python3 scripts/release/collect_licenses.py
python3 scripts/release/package_macos.py
```

当前产物目录由应用版本决定：`.workbench/release-macos-arm64-1.2.0/`，包括.app、ZIP、SHA256和链接检查清单。包为本地ad hoc签名，未公证。Qt运行库随包；视频执行依赖和 Codex CLI 为外部组件。Windows及另一台无Qt机器未验收；实际发布与模型联调结果见 [STATUS.md](STATUS.md)。

完整操作视频与真实成片位于 `.workbench/deliverables/`。演示驱动通过生产界面信号执行真实操作，并连续录制本应用窗口；视频不作为截图门禁。复现与结果见 [测试报告](docs/TEST_REPORT.md)、[演示说明](docs/DEMO_GUIDE.md)、[第三方说明](docs/THIRD_PARTY.md)。

依据：[开发计划](PROJECT_PLAN.md)、[任务状态](tasks.json)、[Studio契约](docs/API_CONTRACT.md)、[原创模板契约](docs/TEMPLATE_CONTRACT.md)、[测试计划](docs/TEST_PLAN.md)。Hypit 许可证按固定版本保留与复核，详见计划§10。
