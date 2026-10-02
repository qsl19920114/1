# M7 specification re-review

Verdict: **PASS — implementation/source review**. No unresolved T033–T035 implementation finding remains in the reviewed working tree. This does not mark the remaining T036 regression, real GUI verification, documentation, packaging, or release work complete.

Baseline: `f088483fa9404bd8a794b9c7723f6c815c83aee3`. Reviewed specification: `docs/plans/2026-10-02-m7-product.md`. Source scope includes the M7 domain/catalog/importer, document/sample controllers, original video-story template, native UI/transport/player, proposal asset validation, and production main wiring. Initial findings are preserved in `spec-initial.md`.

## Resolved findings

- **T033 unavailable sample feedback:** `VideoSample.h:11`–12 carries availability and an error. `SampleCatalog.cpp:17`–25 retains missing/unsafe/oversize/unreadable/unhashable source reasons, returns an unavailable card when an existing distribution has no usable known output, and preserves diagnostics on available cards. `MainWindow.cpp:204`–208 presents the reasons, gives missing-Hypit/import guidance, and supplies no actionable sample path for unavailable entries. Hash deduplication retains all source references.
- **T035 composition transport mode:** `StudioTransport.cpp:23` checks the visible, non-hidden scaler, enabled scrubber, and actual composition marker before enabling native controls. `StudioTransport.cpp:32`–36 repeats those checks at JavaScript execution, closing the polling interval. `StudioTransport.cpp:50` returns through Studio's real `.stage-return` button when reduced preview is requested during artifact preview. The fixed upstream contract is `../hypit/packages/studio/src/ui/stage.ts:238`–244 and 314–345: artifact controls use five-second jumps; composition controls use frames; the scaler's hidden state distinguishes them. The initial finding is resolved in source.

## Requirement checks

| Requirement | Source conclusion |
| --- | --- |
| T033 fixed external sample locations, hashes, source provenance, duplicate handling, unavailable feedback | PASS. Canonical paths stay inside the distribution and no upstream media/source is added to the repository. The three recorded local samples have the same hash and remain visible as three sources of one sample. |
| T034 H.264 MP4, 30 fps, at least 8 seconds, bounded asynchronous immutable copy | PASS. The 64 MiB source limit, queued 256 KiB copy, staged hash, read-only staged file, metadata gate, frame gate, and full-stream decode all precede registration. |
| First 8 seconds in original 1280×720/30 fps/8 second template, source audio muted | PASS. `render.js:13`–14 samples source frames 0–239, uses contain layout, and sets muted playback. `render.js:29`–33 enforces the program's 240 frames and 30 fps. The fixed HyperFrames emitter and Studio shim keep visual video silent. |
| Import failure/cancel/replacement does not commit stale state | PASS. `VideoAssetImporter.cpp:73`–75 and 164–165 use generation guards and cleanup. `DocumentController.cpp:8`–12 guards result delivery; create/open/close cancel imports before replacing document identity. Metadata, timestamp, and decode failures cannot reach commit. |
| Duplicate and manifest safety | PASS. Existing metadata and bytes are compared; mismatching files are retained. Manifest failure removes only a newly owned copy. No unvalidated original is copied after probing. |
| Automatic sample binding only to the created workspace and real writable video field | PASS. `SampleCreationController.cpp:28`–31 requires a ready idle editor, completed import, matching document root and canonical Studio workspace, and an actually editable video binding. Success follows the confirmed writer operation. |
| T035 operation state, compatible asset application, preserved valid snapshot, native playback callbacks | PASS. State gates prevent conflicting document/edit/import operations, asset application requires compatible MIME and a writable field, errors preserve the valid snapshot, and transport callbacks check generation/QPointer lifetime. Import cancellation is available in the native UI and wired in main. |
| Real export completion and film playback gate | PASS in source. Only a production complete task with an existing readable nonempty film enables playback; the export controller reaches completion through media validation. The new player test waits for real video decode dimensions. |
| T036 production main creation/import/binding identity wiring | PASS in source. Main connects the document, Studio editor, sample coordinator, transport, and exporter; the integration test invokes the production window signals and actual playback/seek controls. |

The updated frame gate is at `VideoAssetImporter.cpp:42`–58 and 89–101. It requires exactly 240 finite, nonnegative timestamps, a zero start within 2 microseconds, and both successive and cumulative 1/30-second spacing within 2 microseconds. Its ffprobe phase caps stdout/stderr at 64 KiB, rejects nonzero/abnormal exit, malformed JSON and error diagnostics, has a 30 second deadline, and retains cancellation guards. A nonzero metadata ffprobe exit is rejected even when its JSON fields are otherwise valid (`VideoAssetImporter.cpp:148`). The subsequent ffmpeg phase fully decodes the video stream before commit. Timestamp validation covers the 240 frames consumed by the template; the rest of the accepted stream is covered by full decode and metadata checks.

## Evidence boundary

Previously observed fresh checks passed: focused CTest 3/3 in 10.59 seconds; template Node tests 5/5; catalog recheck after its correction 1/1 in 0.09 seconds. The initial real GUI run passed 3/3 in 25.132 seconds and recorded native seek/play, decoded Studio video, validated export, and reopen. These results predate the final transport/timestamp changes and do not substitute for their updated verification.

`transport-mode-red.log` records the original transport failure in the actual GUI (2 passed, 1 failed, 8.769 seconds). That regression test sets the upstream artifact-mode observable, `.stage-scaler.hidden`, in the real Studio page; it does not claim to navigate a complete artifact selection workflow. The test now also opens the produced film in the new Qt player and requires a decoded 1280×720 video before writing `filmPlayerDecoded: true`.

The reviewer independently ran the frame timestamp command on the real `chat-demo.mp4`: ffprobe exited 0 with empty stderr; 240 timestamps occupied 17,952 bytes; first/last were 0 and 7.966667 seconds; maximum deviation from 30 fps was below 0.000000334 seconds.

At report creation, the updated importer/UI/GUI binaries have been rebuilt, and the parent is running the final regression and real GUI green checks. Their results and release verification remain to be attached by the parent. No final test or release success is inferred from this source PASS.

## 主执行者最终运行证据

随后已完成真实产品E2E（product-e2e-final.log / product-run.json）、最后受影响的UI/启动回归（ctest-quality-final.log）、运行包签名与链接审计（release.json/link-audit.json）、9场景搬移（relocation.json）及最终纯视频工程启动（packaged-video-startup.json）。源码复审与运行验收分别记录，不把源码PASS替代运行验证。
