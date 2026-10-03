# Cross-review closeout

The report-generator P2 finding is resolved in the reviewed current source.

`copy_video` now reuses `verified_decode` only when `expected_hash` is supplied and matches the copied bytes. Without that hash, it runs FFmpeg with `-xerror` on the delivery copy before reporting full-decode success. Both demo video call sites supply no expected hash, so both take the fresh-decode path. Hash-bound official sample evidence remains reusable.

Reviewed existing regression evidence: `docs/evidence/m13/report-decode-red.log` demonstrates that the prior code accepted an unbound swapped-source decode result; `report-decode-green.log` shows both that case and hash-bound official evidence reuse passing after the fix.

Reviewed `.workbench/deliverables-m13/source-manifest.json`, generated at 2026-10-03T11:08:01.592452+00:00. Verdict is PASS. Both `videos/qt-agent-walkthrough.mp4` and `videos/qt-agent-film.mp4` record `fullDecodeExitCode: 0`, `copiedBytesMatchSource: true`, and `fullDecodeEvidence: delivery copy decoded by report generator`, with individual SHA-256 values.

No open findings remain from this cross-review. The original `report-group.jsonl` is retained as the historical finding record. This closeout was read-only and limited to the concrete P2 fix; no tests, builds, media decoding, or model calls were rerun.
