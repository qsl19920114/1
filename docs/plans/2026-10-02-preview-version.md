# 原生工作台可视预览版本确认实施计划

> 按已批准 T042 要求直接执行；采用 executing-plans 与 TDD，不提交。固定外部 Hypit 保持只读。

**Goal:** 仅当中央 composition stage 的实际 HTML document 和媒体匹配编译 Snapshot 时报告预览版本。

**Architecture:** Snapshot 保留服务端 `preview.kind=hyperframes` 的 `previewSrcdoc`，此字段只供 Qt 预览观察，不加入 Agent ContextBuilder。StudioTransport 的独立 DOM 探针检查属性与实际 document，发布带 revision/sourceFingerprint 的 PreviewVersion。MainWindow 将状态转发给 AgentPanel，分别显示编译与可视预览版本。

**Tech Stack:** Qt 6 Core/Widgets/WebEngineWidgets/Test、C++17、固定 Hypit Studio DOM。

## 已核对固定契约

- `../hypit/packages/studio/src/shared.ts:286`：`preview {kind,srcdoc}`。
- `../hypit/packages/studio/src/session.ts:106`：`snapshot.preview.srcdoc` 为生成的 preview HTML。
- `../hypit/packages/studio/src/ui/stage.ts:345,365`：iframe load 后启用 composition scrubber；每个 revision 先取消 ready 再换 srcdoc。
- `../hypit/packages/studio/src/preview/render.ts:27`：materialized HTML 加 runtime shim；素材通过本机 `/__studio/material/` 提供。
- `../hypit/packages/studio/src/preview/runtime-shim.ts:14,231`：运行期修改视觉样式，提供 `__hypitFrameReady` 和 seek/play 接口。
- `../hypit/packages/hyperframes/src/document.ts:662`：编译文字/素材/尺寸/字号可只改变静态 HTML；不能仅比较脚本。
- `templates/story-reel/packages/story-reel/render.js`：原创故事卡使用静态文字、图片和内联字号/颜色。

## Task 1: DTO 与 mapper

Files: `app/domain/Snapshot.h`, `app/backend/hypit/SnapshotMapper.cpp`, `tests/unit/PreviewVersionTest.cpp`。

- [x] 增加 `Snapshot::previewSrcdoc` 和 `PreviewVersion {revision,sourceFingerprint}`（未确认 revision=-1）。
- [x] Mapper 的真实 payload 回归先 RED：只接受 `preview.kind==hyperframes` 且 srcdoc 为 string；缺字段仍保持已有会话兼容但不可确认预览。
- [x] 保留 sourceFingerprint 算法，previewSrcdoc 不成为源指纹，也不进入模型上下文。

## Task 2: DOM 探针与 Transport

Files: `app/ui/StudioTransport.{h,cpp}`, `tests/unit/PreviewVersionTest.cpp`。

- [x] `setSnapshot(const Snapshot&)` 持有预览基线，`previewVersionChanged(const PreviewVersion&)` 发布/撤销确认。
- [x] 公共只读诊断函数 `previewProbeScript(const Snapshot&)` 与轮询使用同一实际 DOM 观察；真实 QtWebEngine 测试可在一次 JS 调用内制造 srcdoc 新属性/旧 document 间隙并观察它。
- [x] 要求 composition 模式（scaler 可见，artifact/back 未打开）、scrubber 就绪、实际 composition 尺寸、readyState=complete、iframe 属性完全匹配 expected srcdoc。
- [x] DOMParser 解析 expected；比对实际内联 SCRIPT/STYLE 原文、稳定静态文本/素材地址/data 属性、静态内联字号/颜色等，运行期可变的 visibility/transform/animation 时钟不作为身份。
- [x] 所有 img complete 且 naturalWidth>0；video readyState>=2、videoWidth>0、无 error/seek；audio 解码就绪；字体加载完成；runtime frame-ready Promise 已结算且无 browser-program error（固定 shim 的 false 表示被后续 seek 取代，reject 才是失败）。
- [x] 顶层 loadStarted/loadFinished(false)、clear、项目替换、新 Snapshot、不同 srcdoc、媒体等待、artifact 模式撤销确认；所有异步回调以 generation 和 QPointer 保护。

## Task 3: 原生 UI 联动

Files: `app/ui/AgentPanel.{h,cpp}`, `app/ui/MainWindow.{h,cpp}`, `tests/unit/AgentPanelTest.cpp`。

- [x] `AgentPanel::setPreviewVersion(const PreviewVersion&)` 独立显示编译快照 vN / 可视预览 vN 或等待，收到不匹配 revision/fingerprint 的确认不可显示当前已确认。
- [x] MainWindow 每次 showSnapshot 调 setSnapshot；真正 WebEngine attach 后也传当前 Snapshot；转发 Transport previewVersionChanged 并供 --verify-startup 监听。
- [x] 单元回归验证等待→匹配确认→新快照等待→旧回调不能确认；保留最近导出指纹过期判断。

## Task 4: 独立真实 QtWebEngine 验证

Files: `tests/preview-version.cmake`, `.workbench/preview-version-test/CMakeLists.txt`, `tests/unit/PreviewVersionTest.cpp`, `docs/evidence/m8/preview-version-red.log`, `docs/evidence/m8/preview-version-green.log`。

- [x] 独立 CMake 链接 domain + mapper + StudioTransport + Qt WebEngine/Network/Test，不写根 tests/CMakeLists，通知 root include 新测试。
- [x] 真实 HTML iframe / 本机图片测试：旧 document + 新 srcdoc 属性间隙，加载后同版本确认，不同 HTML拒绝，视频未 ready，artifact 模式切换，加载失败/clear/版本替换保护。
- [x] 先编译最小 API 骨架并跑 RED，再实现 GREEN；所有测试检查真实 DOM，不截图。
- [x] 运行 AgentPanelTest 及 PreviewVersionTest 完整测试，核对退出码和日志；root 负责另在真实校园故事工程执行 .app --verify-startup 记录模型/实际影像/预览确认版本；独立测试已完成，主 app 验证待 root 结果。

Run (isolated): `cmake -S .workbench/preview-version-test -B .workbench/preview-version-test/build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt`，`cmake --build .workbench/preview-version-test/build -j 4`，`QT_QPA_PLATFORM=offscreen .workbench/preview-version-test/build/preview_version_test`。


## 验证记录

- 第一轮 RED：`docs/evidence/m8/preview-version-red.log`，2 PASS / 10 FAIL（Mapper/Transport 最小 API 尚未实现）。
- 边界 RED：`docs/evidence/m8/preview-edge-red.log`，14 PASS / 2 FAIL，真实复现 `%2/%3/%4` 文本被替换及被后续 seek 取代的已结算 Promise 无法确认。
- 最终 DOM GREEN：`docs/evidence/m8/preview-version-green.log`，16 PASS / 0 FAIL，退出码 0；真实 QtWebEngine iframe、图片解码和各失败路径，无截图。
- 面板 RED/GREEN：`docs/evidence/m8/preview-ui-red.log`（2 FAIL）、`docs/evidence/m8/preview-ui-green.log`（24 PASS / 0 FAIL），退出码 0。
- 测试使用 `QT_QPA_PLATFORM=offscreen QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu`；仅测试进程，未更改产品环境。
- QtChromium 子进程在 macOS 需要系统 IPC，真实 DOM 测试运行经自动权限审核允许。
- `MainWindow::previewVersion()` 与 `previewVersionChanged` 已接通；root 已 include `tests/preview-version.cmake`，主 app 编译及真实工程验收由 root 在共享最终 build 上执行。
