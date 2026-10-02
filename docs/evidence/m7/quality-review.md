# M7 独立代码质量复审

结论：**源码质量审查 PASS；未发现未解决的高置信度 P0/P1/P2 缺陷。**

基准：HEAD `f088483`。范围：当前工作区 36 个已跟踪及未跟踪源码、模板、测试、构建和发布文件；初审按4组完整审查，复审核对原生定位修复及其直接调用链。未执行 git add/stage，未修改实现代码。

## 初审问题关闭

初审 P2：原生定位仅连接 `sliderReleased`，键盘与滚轮不会定位。修复位置：`app/ui/MainWindow.cpp:149`。

现已监听 `actionTriggered`；键盘/滚轮使用尚未传播到 value 的 `sliderPosition`，鼠标拖动期间抑制中间 `SliderMove`，释放时保留原有定位。

临时 Qt 6 探针使用与产品相同连接，实测全部通过：

- Right：提交帧91。
- 滚轮：提交帧94。
- 预览轮询 `setValue(90)`：未发送任何定位。
- 拖动到105：中间未提交；释放后提交帧105。

探针位置：`/private/tmp/m7-quality-review/slider-probe`。真实 GUI 的修复前回归记录 `docs/evidence/m7/keyboard-seek-red.log` 在 Studio DOM 未前进断言处失败；新增 ProductE2E 使用真实 QTest Right 和 QWheelEvent，并回读 Studio scrubber 判断两种定位生效。

## 审查覆盖

覆盖逻辑、业务语义、安全、并发、健壮性、性能及 JavaScript 专项；核对工程 generation、信号重入、取消与异步回调、受控路径、视频元数据及前240帧时间戳、暂存副本及全视频流解码、素材允许列表、模板静音前8秒、Studio artifact 模式禁用，以及1.1.0发布独立目录和现有1.0.0保留。

## 验收边界

本 PASS 仅代表源码审查及定位信号修复已核对。最终真实 ProductE2E、发布包更新及搬移验证仍由主执行者运行并依据实际结果记录；本报告不把源码 PASS 当作 T036 全部完成或发布验收完成。

HTML：[quality-review.html](quality-review.html)

## 启动验证媒体条件补充复核

窄范围复核 `app/main.cpp:232`、`:236`、`:272`，未发现新缺陷。纯视频工程现在允许 `imagesReady=false` 且 `previewVideosReady=true`；通过条件为存在至少一个图片或视频、全部图片加载完成且全部视频已取得可显示帧，再结合编译 composition 就绪。只有图片的既有工程仍使用相同 `imagesReady` 语义。无媒体、损坏图片或尚未就绪视频不会通过；异常 DOM 读取返回空结果而继续等待。新增报告字段为媒体条件补充，既有字段和格式标识保留。

这次改动仅影响启动验证轮询和结果记录。打包后的真实视频工程 `--verify-startup` 与标题卡搬移结果由主执行者另行实测；本补充仅为源码复核结论。

## 停止导出观察后的状态补充复核

窄范围复核 `app/ui/MainWindow.cpp:182` 至相关操作启用判断，以及 `app/main.cpp:165`、`:170`、`:200` 与 `ExportController::stopObserving/clearProject/startExport`。已修复停止观察后被 `task.active=true` 永久阻止关闭、切换工程和配置的问题；停止观察使控制器 `isBusy=false`，已登记的 Build 可继续保持 active 并供以后恢复观察。正在 plan/build/get/validation 的观察阶段仍阻止切换；编辑和视频导入保护保留。

新增导出仍以 `m_exportActive` 和控制器 `busy || task.active` 阻止重复启动。关闭工程调用的是停止观察及清理本地当前工程状态，不会发出 Build cancel；持久化任务记录可在重新打开后恢复。已阅读针对 stopped active 状态的修复前 RED 记录与新增回归断言；最终 focused GREEN 结果由主执行者另行记录。此补充源码审查未发现新缺陷。

## 主执行者最终运行证据

随后已完成真实产品E2E（product-e2e-final.log / product-run.json）、最后受影响的UI/启动回归（ctest-quality-final.log）、运行包签名与链接审计（release.json/link-audit.json）、9场景搬移（relocation.json）及最终纯视频工程启动（packaged-video-startup.json）。源码复审与运行验收分别记录，不把源码PASS替代运行验证。
