# Group 1 review

Read-only review of AgentPanel, HistoryPanel, WorkspaceStore, MainWindow/main integrations and related unit/integration bindings against 1091da9 and current untracked test sources.

Completed full-context reading, all seven general dimensions, no C++ language-specific document applicable, severity/confidence self-check. No builds or source changes.

Findings: one P1 (pending task restoration can restore old approval after historical goal reuse), one P2 (valid 8192-byte escaped goals can be silently dropped by the 16 KiB serialized history cap). Both have concrete local call paths and confidence 10/10.

Confirmed current source uses intended 8192 UTF-8 byte limit in history validation and goal composition; earlier 4096-character issue is not reported. Reviewed filtering, visible selection, project canonicalization, busy action guards, explicit-generation-only behavior and history record bindings.
