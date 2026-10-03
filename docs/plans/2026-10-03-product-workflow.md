# FrameLab 1.3 创作体验 Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development. Each implementation is reviewed for requirements, then quality. Continue in the existing dedicated qt-video-workbench branch; its sibling Hypit and prepared builds are part of the local integration environment.

**Goal:** 完成用户确认的六项产品优化，覆盖创作、场景定位、方案审阅、素材、真实反馈和工作区历史。

**Architecture:** 保持 Qt Widgets → domain，main 负责连接 controllers/services；Hypit 固定版本继续作为编译、预览与执行事实来源。所有模型方案仍需本地校验和用户确认，历史和工作区恢复不得恢复执行授权。

**Tech Stack:** Qt 6 Widgets/WebEngine, C++17, CMake/CTest, pinned Hypit, Codex CLI.

## T043 创作流程、审阅和生成反馈

Files: app/ui/AgentPanel.{h,cpp}, app/ui/PlanReviewPanel.{h,cpp}, app/agent/ModelClient.{h,cpp}, app/agent/AgentController.{h,cpp}, tests corresponding agent panel/model tests.

- [x] Add behavioral tests: input/review/running/result stages show relevant actions; edit proposal focuses editable values; before/after strings are escaped and differences emphasized; impacted scenes map exact IDs; partial public model output cannot enable approval.
- [x] Run agent_panel_test/model_client_test to demonstrate failures.
- [x] Implement compact stage card and task progress, collapsible technical details, editable diff cards, public output signal. Only agent_message content may be displayed; reasoning never exposed. If CLI only emits a completed message, show it when actually received, never simulate streaming.
- [x] Verify stage transitions, failure repair/retry, cancellation, bounded output and existing creation approval invariants.

## T044 场景与预览联动

Files: app/ui/SceneStrip.{h,cpp}, app/ui/MainWindow.{h,cpp}, tests/ProductWindowTest.cpp.

- [x] Test scene list deduplication by start/end range, scene activation emits an actual component ID and frame; selecting a scene selects that component and seeks through StudioTransport; playback highlights active scene.
- [x] Implement image-backed scene thumbnails, text fallback, time/selection labels and component-click seek. Preserve exact compiled IDs, clamp frames and ignore stale preview state.
- [x] Run product_window_test and real Studio seek verification.

## T045 素材管理

Files: app/services/AssetService.{h,cpp}, app/services/ProjectStore.{h,cpp}, app/controllers/DocumentController.{h,cpp}, app/ui/MainWindow.{h,cpp}, new asset panel/helper files, tests asset/document/product tests.

- [x] Test sequential mixed batch import, valid-prefix preservation on failure/cancel, missing-file repair by original content hash, rejecting traversal/symlinks/changed bytes.
- [x] Add multi-file chooser/drop target and extended selection. Preview image/video; replace selected component through existing writable inspector field and shared undo. Make selection/target explicit.
- [x] Permit inspecting a structurally valid manifest with missing media for repair; do not activate incomplete project. Restore verified original bytes at original asset path atomically, then reopen normally.
- [x] Verify import, replace, missing file recovery and cancellation with tests and local fixtures.

## T046 工作区与历史

Files: app/infrastructure/WorkspaceStore.{h,cpp}, app/ui/MainWindow.{h,cpp}, app/main.cpp, app/ui/HistoryPanel.{h,cpp}, tests workspace/history/product tests.

- [x] Test bounded persisted recent projects, geometry/splitters and per-project playhead; reopen with missing project paths handled explicitly; never auto-play or auto-execute.
- [x] Store UI state at application data path (isolated temporary path in tests); restore playhead after transport readiness, clamp to duration, user selection overrides pending restoration.
- [x] Persist bounded per-task history snapshots and display task/change/export records in one tab; details show goal, real steps/results and output path. Open existing films and reveal files; restore plans through existing safe review mechanism only.
- [x] Run tests for project switching, malformed history, stale assets and history export links.

## T047 Integration and delivery

Files: CMakeLists.txt, app/main.cpp, STATUS.md, tasks/, docs/, scripts/release scripts as needed.

- [x] Configure with Qt prefix, build and run complete CTest. Fix actual failures before rerunning relevant tests.
- [x] Perform real local Hypit workflow: load copied project, select scene, seek, import/apply image, inspect review, execute edit, verify current preview, save/reopen workspace/history, export/decode if changed render path requires it.
- [x] Run spec review then code quality review; resolve findings, retain evidence.
- [x] Package 1.3 app and source; verify relocated startup with prepared config; update local launcher.
- [x] Update development docs and evidence, git diff --check, commit and push qt-video-workbench. Report verified results and real limitations.

## Verification commands

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build -j 6
ctest --test-dir build --output-on-failure
```

All completion checkboxes require observed results. Tests using real Codex remain opt-in; no paid Hypit provider is needed. No screenshot acceptance is required by the user.
