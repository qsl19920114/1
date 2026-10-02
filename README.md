# Qt 视频创作工作台

Qt 6 Widgets 视频工程应用。当前完成 **M2/G2**：原创图片标题卡、工程创建/保存/重开、不可变图片导入，以及真实 Studio 连接与预览。进度见 [STATUS.md](STATUS.md)。

## 构建与启动

验证平台：macOS 15.7.7 arm64、Qt 6.11.2（含 WebEngineWidgets/Test）、CMake 4.4.3、Hypit 0.2.10。M2 实测 Node 25.8.2；M0/M1 历史环境为22.22.1。本轮未安装或修改全局环境。

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build -j4
./build/app/qt-video-workbench
```

### 创作流程

1. 点击“新建工程”，输入名称并选择保存位置，创建独立的标题卡工程。
2. 点击“导入图片”，选择 PNG/JPEG。素材列表显示名称、真实尺寸；原图保持不变。
3. 双击素材复制相对路径。在中间 Studio 中选择图片标题卡，将其“图片路径”改为该路径。**导入与应用到画面是两个动作。**标题、副标题、主题色也可由 Studio 修改。
4. 网页修改后点击“刷新会话”，更新右侧原生只读属性。当前尚未实现原生写回和撤销。
5. “保存工程”保存应用元数据，不触发导出。关闭后通过“打开工程”选择 `workbench.qvw.json` 继续工作。移动整个工程目录后仍可重开。

模板为1280×720、30fps、8秒，包含本项目原创的文字/图片布局、简单出现动画和几何占位图。新工程包含自己的源码、Run、Runtime、原创包和素材，不依赖上游示例目录。

也可直接指定工程清单：

```bash
./build/app/qt-video-workbench --project="/绝对路径/校园社团介绍/workbench.qvw.json"
```

“打开 Run…”保留高级入口，选择既有 `.svrun`、workspace 和本地 Runtime。命令行对应 `--run`、`--workspace`、`--runtime`；与 `--project`、离线 `--session` 互斥。

## 工程、素材与配置

- 清单仅存元数据与工程内相对路径；源文件仍是编辑事实，不维护第二套时间线。
- 素材按实际内容解码，使用 SHA-256 命名、复制原始字节并去重。限制为4000万像素、64MiB；清单 `settings.maxAssetBytes` 可进一步降低字节上限。
- 创建不会覆盖非空目录；保存使用原子替换；素材登记失败不发布半完成索引。详情见 [架构与格式](docs/ARCHITECTURE.md)。
- `config/version-lock.json` 中 `hypit.distributionPath` 相对仓库根目录解析，默认 `../hypit`。上游依赖仍由外部 Distribution 提供，不复制进工程。
- `--config` 或“选择配置”切换版本锁；`--log` 指定 JSONL 日志路径，默认在 Qt AppDataLocation。
- Runtime 只接受已验证的本地 media/hyperframes Provider。关闭工程会结束应用启动的 Studio 进程组。

当前开发版从构建时指定的仓库 `templates/` 读取可信模板；独立发布包及 Windows 进程树管理属于 M6，尚未验收。

## 验证（不截图）

```bash
ctest --test-dir build --output-on-failure
node --test tests/template/title-card.test.mjs
./build/tests/project_e2e_test
```

最后一项需本机监听端口权限及实际 Hypit 依赖。它使用生产 Controller/Service 创建工程、导入图片、移动后重开，再读取真实 Studio 会话，并核对图片 HTTP 响应与导入字节完全一致。还验证切换无效工程时旧会话会停止。

结果见 [M2证据](docs/evidence/m2/README.md)。旧 `--selftest --out` 和 M1 截图脚本仅保留为兼容工具，不作为当前验收步骤。

## 后续开发

下一阶段 M3：原生字段写回、409/422恢复、撤销/重做和冲突保护；M4：真实构建任务与MP4导出验收。当前不宣称应用内导出已完成。

依据：[开发计划](PROJECT_PLAN.md)、[任务状态](tasks.json)、[Studio契约](docs/API_CONTRACT.md)、[原创模板契约](docs/TEMPLATE_CONTRACT.md)、[测试计划](docs/TEST_PLAN.md)。Hypit 许可证按固定版本保留与复核，详见计划§10。
