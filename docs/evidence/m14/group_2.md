# Group 2: character handoff, material preparation and showcase

Read-only quality review of current changes against HEAD b850e8f. Reviewed MainWindow/AgentPanel handoff and scope-label changes, associated unit tests, prepare_character_materials.py, build_character_showcase.py, CharacterShowcaseDemo.cpp, CHARACTER_SHOWCASE.md and available m14 evidence. Applied LOGIC, BUSINESS_SEMANTICS, SECURITY, CONCURRENCY, ROBUSTNESS and PERFORMANCE checks; no style-only findings.

No unresolved concrete P0–P2 findings in this group. Production handoff checks registered single visible compatible asset and editor/backend/busy/restoring gates. composeCharacterGoal keeps generation and approval explicit, clears pending approval through the existing cancellation chain, and scopes the subsequent request to the selection. Extraction verifies bounded allowlisted regular members against pinned image hashes and source evidence; uses fixed output filenames and preserves original bytes. Gallery verifies delivered movie and recording hashes against producing evidence and uses controlled relative links.

Execution limits: no builds, source edits, model calls or network actions were performed by this reviewer. Existing character-showcase.log recorded a driver failure (The Source changed outside Studio); root was informed. Current source now includes stable() refresh/preview settlement before setup edits; its runtime outcome remains pending. Do not infer demonstration PASS from this static review. Original materials and actual run artifacts have separate evidence; this review does not certify model execution.

Cross-review found a separate fullscreen-close defect in MediaPlayerDialog, saved in group_2_crossreview.jsonl / group_2_crossreview.md.

Final rereview: the fullscreen-close cross-review finding is fixed and its new focused fullscreen-close case passes, but player-close-green.log then records SIGSEGV in fullscreenEscapeAndCloseCleanup; the suite is not certified green. See group_2_crossreview.md for RED/GREEN evidence. No additional source-confirmed P0–P2 findings; the player test crash requires root investigation, and actual full demonstration remains pending.

Final evidence supersedes the intermediate crash: reread fixed fullscreen Escape keyPressEvent and guarded test; docs/evidence/m14/player-final.log shows 7 passed, 0 failed in 10536 ms. No unresolved Group 2 or cross-review findings. Character demonstration execution remains pending and is not certified by this review.
