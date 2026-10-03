# Cross-group and report review

Reviewed Group 2 summary, catalog and VideoSample contracts, MainWindow/main callers, production AgentWorkbenchBridge and demo, plus scripts/showcase/build_experiment_report.py and docs/EXPERIMENT_REPORT.md.

No additional defect found across catalog → MainWindow → bridge/demo. The optional catalog parameter reaches startup and configuration reload, new metadata is initialized for legacy fallback samples, unavailable entries cannot be used for creation, and the short-clip restriction agrees with the fixed eight-second template. Asset handoff continues to use registered project assets and the current editable component; no catalog entry bypasses the import validation path.

One P2 report-generator finding is recorded in report-group.jsonl: demo video full-decode evidence is inherited without a digest linking the evidence to the current source bytes. Existing official sample digests provide that linkage; the two demo videos currently do not.

Portable links: generated Word and HTML media, image and evidence targets are relative, and generated links/anchors are validated against the staged delivery. Absolute original paths retained inside JSON are provenance records, not media playback dependencies. HTML user-derived text is escaped. Documentation accurately distinguishes scripted UI from manual use, original application/template work from upstream assets, final-run request counts from development totals, and this demo from the full test suite. Its full-decode evidence wording should be updated with the finding fix.

Read-only static review; no builds, model calls, source changes, report regeneration, or media decoding performed. No independent claim is made about current output playback or runtime success.
