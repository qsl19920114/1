# T056–T058 independent specification review

Reviewed 2026-10-03 against `docs/plans/2026-10-03-character-showcase-player.md`. User scope is confirmed: replace a character image/video asset while preserving layout and animation. This review only read source and existing evidence; it ran no build, demonstration, model request, or provider call.

## Result

**Implementation specification review approved; demonstration execution evidence pending.** This is a static specification review, not a runtime PASS for T058. Final source was reread after the idle scope-label and player loop-control fixes.

## Resolved requirement gap

1. **T057: first handoff displayed the full internal component ID.** Initially `app/ui/AgentPanel.cpp:86–88` directly set `agentTaskScope` from `m_selectedEntity` on scope-checkbox changes, bypassing the readable-label formatter and leaving the tooltip unset. The implementer reproduced this with the fresh idle handoff assertion in `tests/unit/AgentPanelTest.cpp:106`; `scope-red.log` contains the expected failure. Final source now calls `updateScopeLabel()` on idle checkbox changes and derives the idle scope from the checkbox and selection, using the component label and full-ID tooltip. Static rereview confirms the concrete gap is fixed. Inspected `targeted-green.log`: AgentPanel, ProductExperience and MediaPlayer all pass, 3/3 in 12.00 seconds.

## Integration and execution evidence

- `tests/CMakeLists.txt` now registers the explicit opt-in `character_showcase_demo` executable, linked to the production workflow library; it is deliberately outside routine CTest.
- Planned `docs/CHARACTER_SHOWCASE.md` now describes the confirmed scope, real execution procedure, provenance, player and delivery layout. Its interview swap-host provider statements were checked against local upstream `swap-host.svml:57,125` and `hypit.runtime.json:10–16`.
- Final player source latches readiness only after an actual decoded frame and preserves Stop during temporary loop seeking/buffering. Errors and shutdown revoke readiness. Existing `player-ctest.log` records the targeted player test passing in 10.31 seconds; its current source exercises 12 loop/stop cycles.
- `docs/evidence/m14/character-showcase.json` and the completed three-film gallery were not yet available. T058 cannot be marked runtime complete until the opt-in demonstration succeeds and its output is packaged. Existing material-preparation evidence does not substitute for these checks.

## Requirements traced in source

| Requirement | Static evidence and limits |
| --- | --- |
| Registered material, compatible single selection, scoped handoff | `MainWindow::compatibleAssetField()` and `prepareSelectedAssetGoal()` require a visible single image/video asset and an editable matching component field; the chosen binding is matched against the project asset registry. Character action shares busy/restoring gates with the existing asset handoff. |
| Explicit generation and approval; preserve other fields | `AgentPanel::composeCharacterGoal()` prepares a scoped request, clears pending review via `composeGoal()`, and explicitly asks to preserve text, timing, layout, colors, animation and other components. It emits neither Generate nor Approve. The production workbench bridge retains the real controller approval chain. |
| Three real variants, two model requests | Demo imports three distinct prepared portraits, exports baseline, then clicks the production character handoff/Generate/Approve twice. Its real `ModelClient` uses the configured Codex executable. These are code paths awaiting execution, not completed requests. |
| Before-approval and unchanged-property guarantees | Demo compares source files and fingerprint before approval, requires exactly one operation on the image field with the exact registered binding, compares nonimage inspector values after application, and checks a changed source fingerprint. The title-card template has one image field. |
| Actual preview identity | Demo waits for the current fingerprint and confirmed `StudioTransport` preview. Transport checks iframe srcdoc, document structure, text/material attributes, stable presentation, decoded media and frame-runtime settlement. |
| Fully decoded films | Demo waits for production export phase `complete` and matching source fingerprint. `ExportController` reaches complete only after `MediaValidation` succeeds. Gallery requires three distinct film hashes and binds delivered media to demo hashes. |
| Honest own-window recording | `ShowcaseRecording` grabs the actual application/player widget, stores observed frame intervals in ffconcat, encodes a continuous video, and decodes that recording. Gallery checks duration against recorded elapsed time. Runtime output still needs inspection. |
| Native reusable player | Widget owns an isolated WebEngine profile/page, native play/pause/stop/±5 s/seek/time/volume/mute/rate/loop/fullscreen controls, observed DOM state, load/decode errors, revision-guarded QPointer callbacks, and teardown. Dialog preserves the constructor and handles fullscreen/Escape/close. Final loop-related source and the existing targeted test log were inspected. |
| Provenance and provider boundary | Preparation reads only allowlisted bounded regular archive members, pins original PNG hashes and evidence-source hashes to the locked Hypit commit, preserves original bytes, and records URL/member/hash/attribution. It identifies GPT-image/Seedance/Hypihub declarations without executing them. Black-cap source attribution explicitly avoids certifying an unverified generation history. |
| Honest gallery scope | Gallery says the demonstration substitutes existing pictures, uses two actual Codex requests, and does not claim new identity-regenerated performances or newly generated upstream media. Its controlled links are relative; the editable project is reported separately as a local path. |

No unresolved concrete implementation specification gaps were found in the inspected source. This does not certify the pending real demonstration, its resulting gallery, or the still-unfinished release tasks.

## Main-task runtime verification after review

Final real character demonstration completed: character-showcase.log 3 QtTest entries passed (setup/scenario/cleanup),125.590 seconds. Its producing JSON records2 actual Codex requests,2 explicit approvals,3 distinct1280×720/8-second films, preserved non-image properties, preview fingerprint agreement, actual player actions and stopped Studio. Main task visually inspected recorded player frame560 and actual exported frames. Gallery generation plus4 media hashes/11relative links passed in showcase-delivery.json. The real recording is118.5seconds; source wait timing retained. This section is main-task evidence, not an assertion of additional reviewer reruns.
