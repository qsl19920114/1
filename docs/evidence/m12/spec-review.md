# M12 requirements review

Reviewed 2026-10-03 against HEAD `1091da9` and the current uncommitted T048–T050 implementation, including untracked ImportResultsPanel, history-efficiency.cmake, unit tests and CreationEfficiencyE2ETest. Scope: requirements inspection only; no builds or test execution performed by this reviewer. T051 validation and delivery remain in progress; this report does not certify packaging, relocated startup, commit, push, source archive or launcher delivery.

## Final requirements verdict

**PASS for T048–T050 after final source and evidence recheck. No open actionable requirement findings.** T051 native package and delivery validation remain pending and are outside this verdict.

## Closed finding

**CLOSED — P2: Goal reuse rejected valid existing goals above 4096 characters.**

The original `QString::size()` bound prevented reuse of a valid 5000-character ASCII goal, while allowing some multibyte goals that generation rejected. Both current predicates now use the controller's **8192 UTF-8 byte** limit: `app/ui/HistoryPanel.cpp:27`, `app/ui/AgentPanel.cpp:107`, and the existing generation contract at `app/agent/AgentController.cpp:72`. The history tooltip at `app/ui/HistoryPanel.cpp:78` and the T048 plan now agree with that contract.

Reviewed `goal-limit-red.log`: the added cases reproduced the ASCII rejection and multibyte over-limit acceptance before the fix. Reviewed `goal-limit-green.log`: AgentPanelTest reports 42 passed / 0 failed and HistoryPanelTest reports 15 passed / 0 failed. The tests check 5000 and 8192 ASCII bytes, 8190 Chinese UTF-8 bytes, rejection at 8193 bytes for both ASCII and Chinese, exact whitespace/newline preservation, and no automatic generation or approval. Existing WorkspaceStore save/reload coverage retains a complete 12288-byte Chinese goal while dropping oversized details, so storage does not impose the former UI character limit.

## Other inspected requirements

No additional actionable requirement gap was identified in these paths:

- History keyword/type/current-project filtering, visible selection preservation, empty results and legacy goals: `app/ui/HistoryPanel.cpp:58` onward; `tests/unit/HistoryPanelTest.cpp`.
- Reuse copies structured text, revokes visible old review and scope, refuses busy states, and emits no generation/approval: `app/ui/AgentPanel.cpp:106`; `app/ui/MainWindow.cpp:212`; main bindings at `app/main.cpp:94`; `tests/unit/AgentPanelTest.cpp`.
- Asset name/type filtering, hidden selection invalidation, reset on root change, same-root refresh and visible selected replacement/preview: `app/ui/MainWindow.cpp:244`, `:345`, `:358`, `:366`; `tests/unit/ProductExperienceTest.cpp`.
- Per-entry import reports, incomplete-only retries, session/project gating, cancellation retention, 64-file bound, and generation checks around import notifications: `app/controllers/DocumentController.cpp:61`, `:85`, `:107`, `:136`, `:150`, `:158`; `app/ui/ImportResultsPanel.cpp:20`; `tests/unit/AssetLibraryTest.cpp`; `tests/unit/ImportResultsPanelTest.cpp`.
- Main application report/retry wiring and removal of the log-only error recovery prompt: `app/main.cpp:231`.
- WorkspaceStore preserves the structured goal when trimming large details: `app/infrastructure/WorkspaceStore.cpp:42`; save/reload regression in `tests/unit/WorkspaceStoreTest.cpp:15`.

Existing real integration evidence was read, not regenerated: `efficiency-e2e.log` records 3 passed / 0 failed; `efficiency-e2e.json` records initial import 2/3, repaired incomplete retry 1/1, one visible filtered asset, reuse without execution, a real asset write and confirmed preview fingerprint. The corresponding harness is `tests/integration/CreationEfficiencyE2ETest.cpp:60` onward. The first complete full-suite evidence in `ctest-final.log` records 36/36 passed in 183.69 seconds. Goal-boundary fix validation is independently recorded in `goal-limit-green.log`; this review does not substitute for the coordinating agent's remaining final build and native package gates.

## Root delivery verification

After the independent requirements review, root completed final CTest36/36 (184.03s), package rebuild with both quality fixes, relocatednative1.4.0 startup, ownership cleanup and artifact integrity checks. Evidence: ctest-quality-final.log, packaged-native.json, delivery-integrity.json. This section records root execution, not additional reviewer test runs.
