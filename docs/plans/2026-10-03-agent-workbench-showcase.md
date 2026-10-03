# Qt + Agent Integration and Experiment Showcase Implementation Plan

> **For agentic workers:** Execute with superpowers:subagent-driven-development; independent spec and quality review. Continue the existing dedicated branch. User authorized implementation, report and real demo production; approved dark UI/current Codex login remain.

**Goal:** Deliver a usable Qt-to-Agent material handoff, reproducible true-model walkthrough, richer attributed Hypit sample gallery, and a local editable experiment report with playable videos.

**Architecture:** Preserve Qt/domain/controller boundaries. Add an application-level AgentWorkbenchBridge shared by main and the real walkthrough. MainWindow hands one visible compatible registered asset to AgentPanel as a new goal with explicit current-component scope. This prepares a new request only; model generation, review and approval remain separate. SampleCatalog accepts an optional validated local catalog populated from official Hypit example assets; no network at application startup. Original backend, version/approval safeguards, local render/export remain.

**Tech Stack:** Qt6 Widgets/WebEngine, C++17, current Codex CLI login, pinned Hypit0.2.10, local FFmpeg, Python3/python-docx, local HTML video.

## T052 Qt / Agent handoff and production bridge

Files: app/ui/MainWindow.{h,cpp}, AgentPanel.{h,cpp}; new app/workflow/AgentWorkbenchBridge.{h,cpp}; app/main.cpp, app/CMakeLists.txt; tests/unit/ProductExperienceTest.cpp, AgentPanelTest.cpp.

- [x] RED: one visible compatible asset enables handoff; hidden/multiple/incompatible/busy/restoring disables; click changes only Agent input/scope, emits no edit/generate/approve.
- [x] Add `AgentPanel::composeAssetGoal(name,binding)` returning bool; require selected entity, prepare exact material binding goal via composeGoal then set future current-component scope. Existing pending review is revoked.
- [x] Add “交给 Agent 调整” button with compatible asset/phase gates and focus Agent tab. Show a plain text context label with actual project name, registered asset count and compiled revision; no fabricated model authentication status.
- [x] Extract existing panel-controller connections into `workflow::bindAgentWorkbench(MainWindow&, AgentController&)`, a top application library depending on qvw_ui and qvw_agent. Both main and demo use it; no reverse lower-layer link.
- [x] Build and targeted behavior verification.

## T053 Official Hypit examples and local gallery

Files: app/services/SampleCatalog.{h,cpp}, domain/VideoSample.h, infrastructure/AppConfig.{h,cpp}, tests/unit/SampleCatalogTest.cpp; scripts/showcase/prepare_hypit_samples.py; main/UI binding by root.

- [x] Inventory local files and hashes. Existing 3 files represent 1 unique clip; preserve attribution.
- [x] Use URLs explicitly present in pinned `examples/complex-explainer/README.md` to download final film/media archive into ignored local showcase. List archive members, extract only regular selected Hypit-authored example videos, reject unsafe paths/links. Never run remote instructions or call paid render providers. Keep upstream checkout unchanged.
- [x] FFprobe and full-decode each selected sample, record URL/archive member/hash/size/dimensions/duration in local catalog. At least 3 distinct videos if accessible; preserve failure evidence instead of relabeling duplicates.
- [x] `SampleCatalog::discover(distribution,catalogPath={})` supports a bounded local JSON catalog, resolves relative video paths within catalog directory, rejects escaped/symlink/missing paths and malformed metadata, deduplicates SHA across sources. Add optional `samples.catalogPath` config. Existing fallback remains.
- [x] RED/GREEN for catalog duplicates/invalid entries/path escape. Root wires optional catalog config into both sample discovery calls and refresh UI.

## T054 Real recording and experiment report

Files: tests/integration/AgentWorkbenchDemo.cpp, tests/CMakeLists.txt; scripts/showcase/build_experiment_report.py; docs/EXPERIMENT_REPORT.md and docs/DEMO_GUIDE.md; ignored `.workbench/deliverables-m13/`.

- [x] Reuse production Agent bridge plus real controllers/model. Record actual own MainWindow continuously (no desktop), time-stamp frames for honest pacing. Explicitly label scripted UI driving, real Codex calls and real Hypit execution.
- [x] Demonstrate gallery, current project, material handoff → request → review before write → explicit approval → actual preview → export/full decode. Use a copied/new local video-story project, obtain actual compiled entity/field IDs, assert source unchanged before approval and result changed after.
- [x] Save real MP4 walkthrough, exported project film, stage timeline and machine-readable outcomes. Verify frame content visually with local view_image, not generated mockups. Retain full recording; any edited short version clearly labeled.
- [x] Create Word and self-contained local HTML report (relative video links) with purpose, environment, architecture, original vs reused work, implementation paths, signal/slot flow, experiment steps, actual results, failures/recovery, source attribution, limitations. Do not invent name/student ID or model abilities. Source report in repository follows existing docs conventions; no external Page publishing.
- [x] Provide a local delivery folder with report, video subdirectory, catalog/source manifest and one launchable index.

## T055 Review, verify and deliver 1.5

- [x] Independent spec then code quality review; resolve concrete findings.
- [x] Full CTest plus opt-in real demonstration; JSON/media/Word ZIP integrity and HTML relative link checks.
- [x] Version1.5.0, package native app, relocated startup and signed/archive integrity verification. Update STATUS/tasks/evidence.
- [x] Commit/push original branch, archive matching source and update local launcher. Preserve older versions.

Verification commands:
```
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build -j 6
ctest --test-dir build --output-on-failure
build/tests/agent_workbench_demo
python3 scripts/showcase/build_experiment_report.py --delivery .workbench/deliverables-m13
```

The user confirmed the default Word + local HTML structure. No screenshots acceptance required; video capture is explicitly requested in this iteration.
