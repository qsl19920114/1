# M7 独立代码质量审查

基准：HEAD `f088483`；范围：当前工作区已跟踪改动与未跟踪新增源码，完整文件审查。未暂存代码。

结论：发现 1 个 P2 缺陷，未发现高置信度 P0/P1。共审查 36 个源码/构建/发布文件，按 4 组串行审查；覆盖逻辑、业务语义、安全、并发、健壮性、性能，及 JavaScript 专项。分组交叉核对了工程身份、状态回调、素材允许列表与版本化发布路径。

## 1. [P2][逻辑错误] 原生定位滑块的键盘操作不会定位预览

- 位置：`app/ui/MainWindow.cpp:148`
- 置信度：10/10
- 问题：原生定位只连接 `QSlider::sliderReleased`。键盘 Right、End 等操作会更新滑块并发出 `actionTriggered/valueChanged`，不会发出 `sliderReleased`，所以没有调用 `StudioTransport::seek`；随后非拖动状态下的 250 ms 轮询会把滑块恢复成实际预览帧。
- 实测：临时 Qt 6 探针中 Right 将值从 90 改为 91，End 改为 239；`sliderReleased` 均为 0，`actionTriggered` 分别为 1、2。探针位于 `/private/tmp/m7-quality-initial/slider-probe`。
- 代码：`connect(m_seekSlider,&QSlider::sliderReleased,m_transport,[this]{m_transport->seek(m_seekSlider->value());});`
- 建议：对键盘/滚轮等用户动作提交定位；拖动仍在释放时提交，轮询赋值阻止反馈；补充键盘定位回归。

## 证据与边界

导入检查已阅读私有暂存副本、64 MiB 上限、H.264/30fps/8秒元数据、前240帧零起点时间戳、全视频流解码、取消、重名比较及保存回滚。Studio 检查已阅读 QPointer/generation 回调保护与 artifact 模式执行前再次校验。模板检查了前240帧 sampling、muted、允许的工程 assets 路径及文案/颜色/动画约束。发布检查了1.1.0动态版本、新目录拒绝覆盖与当前版本 repair 标记。

完整 CTest、真实 ProductE2E 与发布搬移验证由主执行者运行；本审查未把其未完成阶段列为代码缺陷。未报告仅凭外部文件并发改写而推测的 manifest 冲突。

HTML：[quality-initial.html](quality-initial.html)
