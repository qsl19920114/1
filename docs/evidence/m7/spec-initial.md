# M7 initial specification review

Baseline: `f088483fa9404bd8a794b9c7723f6c815c83aee3`; reviewed the current working tree, including untracked M7 source and templates. Scope: T033–T035 and the production wiring in T036 from `docs/plans/2026-10-02-m7-product.md`. T036 release, documentation, and final acceptance work remains outside this implementation review.

Initial verdict: **NEEDS FIX**, two bounded findings. The catalog finding was already corrected while the review was in progress.

## Findings

1. **P2, confidence 10/10 — unavailable samples were silently omitted (T033).** Initial `app/services/SampleCatalog.cpp:17`–19 dropped missing, unsafe, oversize, unreadable, and unhashable sources. Initial `app/domain/VideoSample.h:6` had no availability or error fields, and `app/ui/MainWindow.cpp:207` showed only a generic empty state. This did not satisfy the explicit available/unavailable state with actionable errors. The subsequent `available`/`error` DTO fields, unavailable card, retained skipped-source reasons, and explicit missing-Hypit guidance address this finding; final re-review should confirm the updated tests.

2. **P1, confidence 10/10 — native composition controls remained active in Studio artifact mode (T035).** `app/ui/StudioTransport.cpp:23` originally required a usable scrubber and a composition marker in the iframe, but did not check that `.stage-scaler` was visible. Pinned external Hypit `packages/studio/src/ui/stage.ts:335` hides the scaler when an artifact is selected while retaining the composition iframe. Its lines 238–244 change the previous/next commands to five-second artifact jumps. Consequently, selecting a timed artifact in full Studio leaves Qt controls labelled as frame steps and a slider bounded by the composition's frame count, despite the underlying commands having different semantics. Require composition mode in both readiness polling and command execution, so a mode change during the polling interval cannot execute stale commands.

## Confirmed implementation

- T034 copies a bounded source into an engineering-owned temporary directory in queued 256 KiB chunks, hashes the accepted bytes, makes the staged copy read only, and probes/decodes that copy. It does not validate a mutable original and then import different bytes.
- `VideoAssetImporter.cpp:115` rejects a nonzero ffprobe exit even when its JSON metadata is otherwise valid. Invalid codec/container/frame rate/duration/frame count/dimensions stop before decoding. Nonzero/abnormal ffmpeg exit, error output, output overflow, and deadline failure stop before manifest commit. Cancellation invalidates queued work by generation and removes staging files.
- `DocumentController` cancels imports before create/open/close and protects imported result delivery with document generations. Manifest failure removes only the newly owned copy; duplicate imports compare both metadata and bytes and preserve mismatching existing files.
- `SampleCreationController.cpp:28`–31 waits for import completion and a ready idle editor, matches the document root, compares canonical Studio workspace identity, and selects an actually writable `video` field. Its success signal follows the confirmed Studio edit operation.
- `video-story` is an original 1280×720/30 fps/240-frame composition. Its IR uses source frames 0–239, muted video, and contain layout. The fixed Hypit HyperFrames emitter (`packages/hyperframes/src/document.ts:372`–407) implements the sampling attributes, and Studio's runtime shim (`packages/studio/src/preview/runtime-shim.ts:161`–163) keeps visual video silent.
- Default errors preserve the loaded snapshot. Asset application requires matching registered MIME and a writable image/video field. Film playback requires a completed task and an existing readable nonempty film; completion comes through the production export validator.
- Studio callbacks use generation and QPointer lifetime guards. The scrubber input, play, and frame commands were checked against the fixed external `packages/studio/src/ui/stage.ts:237`–244 and 298–311, rather than inferred from selectors.

## Verification and limits

Fresh focused run: `ctest --test-dir build --output-on-failure -R '^(video_asset_importer|product_window|sample_catalog)$'` passed 3/3 in 10.59 seconds. `node --test tests/video-template-test.mjs` passed 5/5. These unit tests deliberately use fixture processes and are not claimed as real media acceptance.

The later available `product-e2e-initial.log` records real GUI integration passing 3/3 in 25.132 seconds, and `product-run.json` records actual native seek/play advancement, decoded video in the real Studio DOM, production validated export, and reopened project. The review inspected the test implementation: its PASS evidence is written only after those assertions succeed. This report does not claim release verification.

At the initial review snapshot, constant 30 fps was inferred from ffprobe nominal/average rates and frame count. Validation of the first 240 frame timestamps is being strengthened separately and should be checked in the final review.
