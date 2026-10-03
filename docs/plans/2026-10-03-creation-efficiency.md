# FrameLab 1.4 Creation Efficiency Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development with requirement and quality review. Existing dedicated branch and prepared sibling Hypit environment are retained. User authorized continued product iteration; no new provider or style decision is required.

**Goal:** 让用户更快找到素材和历史任务，并直接恢复未完成的素材导入。

**Architecture:** 保持原有 Qt/domain/controller 分层。HistoryPanel 只复制结构化 goal 回到输入，不恢复批准或自动生成；素材过滤只改变可见选择；DocumentController 保留本次会话、当前工程的导入恢复列表并提供真实逐项结果。

**Tech Stack:** Qt 6 Widgets/WebEngine, C++17, CMake/CTest, fixed local Hypit.

## T048 历史查找与目标复用

Files: app/ui/HistoryPanel.{h,cpp}, AgentPanel.{h,cpp}, MainWindow.cpp, tests/unit/HistoryPanelTest.cpp and AgentPanelTest.cpp.

- [x] Add RED tests for keyword/type/current-project filtering, selection preservation, empty matches, and reuse emitting only the full structured goal. Missing legacy goals cannot be guessed from summary strings.
- [x] Implement `HistoryPanel::setCurrentProject(QString)`, `setAvailable(bool)`, `reuseGoalRequested(QString)` and `AgentPanel::composeGoal(QString)` (returns bool). Reusable goals use the existing controller limit of 8192 UTF-8 bytes (including legacy ASCII goals longer than 4096 characters). Reuse preserves text only, clears old visible review and future scope, emits no generate/approve, refuses while busy. Final generation still requires review/approval.
- [x] Wire current project and typed history fields (`kind`, `goal`); focus Agent tab when reuse succeeds. Verify inactive filters cannot keep actionable hidden records.

## T049 素材查找

Files: app/ui/MainWindow.{h,cpp}, tests/unit/ProductExperienceTest.cpp.

- [x] Add RED cases for case-insensitive name/type filters, hidden selection invalidation, zero-match message and filters surviving same-project metadata refresh.
- [x] Add assetSearch and assetType controls, visible/total count and clear-filter affordance. Hidden items are deselected; replacement and preview require a single visible selection. New imported matching assets can be selected; imported nonmatching items stay hidden without becoming actionable.
- [x] Verify actual replacement uses the visible selected path and source switch resets filter state.

## T050 导入结果与恢复

Files: app/controllers/DocumentController.{h,cpp}, app/ui/ImportResultsPanel.{h,cpp}, MainWindow.{h,cpp}, main.cpp, tests/unit/AssetLibraryTest.cpp and ImportResultsPanelTest.cpp.

- [x] Add RED cases for per-entry imported/failed/pending state, retry skipping successes, cancellation retaining unfinished paths, replacement project clearing recovery and busy retry refusal.
- [x] Implement `importReportReady(QJsonObject)` with projectRoot, entries(path,state,error), active/completed/total; `retryIncompleteImports()` retries only failed/unprocessed paths retained by the controller for the same current project. Bound to existing 64-file batch. Closing/replacing a project clears recovery. No persisted automatic import after application restart.
- [x] Show a dedicated results tab with per-file reason, real counts and a gated retry button. Remove log-only recovery UI; existing log remains diagnostic. Snapshot and report notifications stay generation guarded during cancellation/reentrant signals.
- [x] Verify retry with real locally generated image plus failed path repaired on disk, then confirm only incomplete entry is retried.

## T051 验证与交付

- [x] Build all targets and run full CTest; independent requirements and quality review, fix findings.
- [x] Real Qt/Hypit integration verifies filters, goal reuse without execution, recovery and unchanged preview/export semantics on a copied project. Reuse existing native integration harness, keep prior evidence intact.
- [x] Version 1.4.0, build local package, verify relocated startup and output integrity. Update STATUS/tasks/guide/evidence.
- [x] Commit and push qt-video-workbench, create source archive and update local launcher.

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build -j 6
ctest --test-dir build --output-on-failure
```

Acceptance is real local operation and tests; screenshots are not required. Existing dark UI, immutable media, edit transactions and explicit model approval remain the product foundation.

Review adjustment: initial 4096-character reuse bound was rejected after verifying AgentController accepts 8192 UTF-8 bytes. Match the existing generation contract, with ASCII and multibyte boundary regressions.

Completion evidence: docs/evidence/m12/README.md. Final full CTest36/36 (184.03s), real integration3/3, spec/quality recheckPASS, relocatednative1.4.0PASS. Quality review additionally fixed restoring-phase reuse and escaped-goal storage capacity; RED/GREEN evidence retained.
