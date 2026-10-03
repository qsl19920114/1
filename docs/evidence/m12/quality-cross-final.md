# Final cross-group review

Result: no remaining confirmed P0–P2 findings in the reviewed cross-group contracts. Both initial Group 1 findings are addressed by the current source.

Read the review grouping and both group JSONL outputs; inspected the current diff and relevant complete implementations/declarations for AgentPanel, HistoryPanel, WorkspaceStore, DocumentController and the MainWindow/main.cpp integration. Also read the directly connected AgentController status, restore, stop, scope and generation paths and the AgentStatus definition. Read the new/updated HistoryPanel, AgentPanel, WorkspaceStore and ProductExperience test cases. Group 2 implementation/tests/CMake were covered in the preceding group review.

Fix validation by source inspection:
- Restoring tasks: composeGoal now rejects restoring before changing the draft or emitting stop; MainWindow tracks restoring and disables history reuse while that phase is active. The controller’s restoring-to-review emissions therefore cannot replace a draft accepted through the reported restoring path. The new unit cases exercise both panel rejection and window availability.
- Escaped goals: record/load use the same 64 KiB record ceiling. An accepted 8192-byte goal requires at most 49152 bytes when all one-byte characters require six-byte JSON escapes, leaving room for the normal task metadata; oversized detail is reduced without shortening goal. A 3 MiB history limit and oldest-record eviction keep ordinary generated layout/recent metadata within the existing 4 MiB file ceiling. Record/load accounting both include array brackets and commas. New cases cover quoted goals and repeated worst-case escaped goals through save/load.

Cross-group contracts:
- MainWindow records structured kind/goal fields; WorkspaceStore preserves them; HistoryPanel emits the complete stored goal only after the same 8192 UTF-8-byte check used by AgentPanel and AgentController. MainWindow handles composeGoal’s boolean result and main connects the explicit stop/generate/approve paths.
- New history project/availability APIs have matching callers on document display/clear and action updates.
- Import report payload keys/types match both MainWindow and ImportResultsPanel consumers, and retry dispatch reaches DocumentController::retryIncompleteImports through the application binding. The import and history busy gates share the current editor/import/agent/export state.
- Reviewed recovery-reset generation guards and their direct application consumers; no additional mismatch identified.

This was read-only source/test inspection. No builds or tests were run, and no source files were modified by this reviewer. Test execution and final integration verification remain with the root agent.
