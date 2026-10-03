# T054 / T055 final specification review

Date: 2026-10-03. **T054: PASS. T055 inspected verification gates: PASS; release closeout gates remain pending at this review snapshot.** No concrete P0–P2 implementation findings were identified in this scope.

This read-only review extends `spec-review.md` for T052/T053. Requirements were checked against `docs/plans/2026-10-03-agent-workbench-showcase.md`, the current demo/report source, actual final evidence and generated delivery files. No build, model request, renderer or release operation was run by this reviewer. The only repository edit is this review record.

## T054: evidence and delivery

- The accepted run is `20261003-185901` in `agent-workbench-demo.json`, not the earlier recording. Its driver is explicitly described as scripted production widgets/signals, production `AgentWorkbenchBridge`, actual Codex and actual local Hypit. This accurately matches `AgentWorkbenchDemo.cpp`; it is not presented as a manual usability study.
- The demo shows the catalog, new video-story project, import of official ranking video, Qt asset handoff, real proposal generation, review, explicit approval, updated preview, second natural-language title edit, export and history. The material and title plans contain actual compiled entity/field identifiers and the exact requested values. There are two model-generation button invocations and two explicit approval clicks.
- Before the first approval, the demo checks the compiled source fingerprint and source-file contents against the baseline; handoff also leaves the fingerprint unchanged. Before the second approval it checks the post-material fingerprint remains unchanged. After approval it checks the material fingerprint changed, the title inspector contains the requested title, and the preview fingerprint matches the current compiled snapshot. These assertions support `sourceUnchangedBeforeApproval=true`; the report does not mistake model response acceptance for a completed edit.
- Export completes through the production controller and matches the snapshot fingerprint. Evidence records Build `bld_20261003T110001925Z_2908E777F3`, artifact full decode, recording full decode, and stopped owned Studio. The final Qt Test log reports 3 passed / 0 failed; the three count includes Qt Test setup/cleanup, not three distinct end-user experiments.
- `WindowRecording` samples the actual owned MainWindow, records observed frame intervals in `recording.ffconcat`, and keeps model wait time. The final run captured 408 frames over 81.456 seconds; the delivered recording is 81.533333 seconds at 1600×1000. The small final-frame/encoding difference is disclosed. No shortened or accelerated clip is presented as the full recording.
- `recording-correction.json` retains the first run's DPI defect and its correction. `fitted.setDevicePixelRatio(1)` is present. This reviewer used `view_image` on the delivered `images/stage-06.jpg`: the actual Qt window fills the expected recording area, with imported ranking media, changed preview and completed Agent task visible. The report's three images remain supporting frames, not substitutes for the video/assertions.

## Report and media checks

The generated `.workbench/deliverables-m13/` contains `index.html`, `experiment-report.docx`, `source-manifest.json`, copied evidence, three actual capture frames and all seven requested videos:

| Delivered video | Duration | Role |
|---|---:|---|
| `qt-agent-walkthrough.mp4` | 81.533333 s | Full actual application recording |
| `qt-agent-film.mp4` | 8.000000 s | Actual exported video-story composition |
| `official-01.mp4` | 137.066667 s | Official finished explainer |
| `official-02.mp4` | 7.033333 s | Official interview; preview only |
| `official-03.mp4` | 10.033333 s | Official ranking example |
| `official-04.mp4` | 9.033333 s | Official podcast example |
| `local-chat.mp4` | 8.000000 s | Original upstream local sample; duplicates counted once |

Reviewer independently recomputed delivery hashes and checked all `source-manifest.json` file sizes/hashes and source-file hashes against the current files: all matched. The copied final demo JSON is byte-identical to repository evidence. Word ZIP integrity passed; Word retains blank course/name/student-ID fields. HTML contains seven video controls, and every local Word/HTML link and HTML fragment target resolves. The generator records 48 validated links. Full decoding was not repeated by this reviewer: the report transparently distinguishes reused decode evidence tied to identical bytes from the local-chat decode performed by its generator.

The Word/HTML prose covers purpose, fixed environment records, architecture, implementation paths, signal/slot flow, experiment steps, observed results, recovery limitations, provenance and future limitations. The report correctly distinguishes original Qt/controller/templates/approval work from reused Hypit language/compiler/Studio/rendering, Qt, Codex CLI, FFmpeg and official video assets. It explicitly states no image pixels were uploaded and makes no claim of visual understanding. Official URLs/archive members/hashes remain in the source manifest. The seven-second interview remains labelled preview-only; the delivered film uses the eight-second template. The manifest correctly records one audio stream in that film while the report describes the template's muted output, not an absence of audio streams.

`build_experiment_report.py` checks final demo PASS, two model calls, two approvals, before-approval invariance, decode flags, four unique official hashes and source consistency before generating the report. It performs no model request or rendering. It builds relative-link HTML and editable Word from real evidence and distinguishes this experiment's PASS from the entire project's test status.

## T055 verification and remaining closeout

Inspected evidence supports these gates:

- `ctest-first.log`: 36/36 tests pass.
- `build-version.log` and `version-startup-test.log`: version 1.5.0 build and startup regression pass after the version change.
- `packaged-native.json` / `packaged-startup.json`: relocated app under a Chinese path containing spaces, clean initial PATH, confirmed preview/snapshot match, media ready and owned Studio absent after exit.
- `release.json` / `delivery-integrity.json`: 1.5.0 archive hash `89c49bdb27a7f077f1335126e87306ad99b87711346e7897ebe26db28cd08204`, executable/archive consistency, 82 Mach-O binaries and 248 verified license files. Signing is explicitly ad-hoc and not notarized. External Hypit/Node/FFmpeg/browser/Codex requirements are disclosed.

At this review snapshot, `STATUS.md`, `tasks.json` and `docs/DEMO_GUIDE.md` had not yet been updated for T052–T055, and the working tree remained uncommitted. Final quality-review resolution, documentation/task updates, commit/push, matching source archive and updated local launcher must therefore be confirmed by the main task before marking all of T055 complete. These known in-progress release steps are not reported as product defects or silently counted as passed. Preserve older release versions as required by the plan.


## Main-task release closeout evidence

After the review snapshot: STATUS/tasks/DEMO_GUIDE updated for M13, report P2 fixed and independently re-reviewed in cross-review-final.md. Main task verified17 delivery files,18 source-file hashes,48links,3 actual Word images; report-delivery.json records the result. Current local launcher points to1.5 and the final recorded project. Code commit/push and matching source archive are tracked by Git and ignored local release source-delivery.json to avoid self-referential commit hashes.
