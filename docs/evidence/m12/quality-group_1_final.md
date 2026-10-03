# Group 1 final recheck

Rechecked the current source and regression test changes without running builds or editing repository files.

Resolved findings:

- P1 old approval after reuse during restoration: AgentPanel::composeGoal now rejects restoring before any mutation or signal. MainWindow tracks restoring explicitly and disables history actions until a later status clears it. Normal open/close actions remain governed separately. Regression coverage includes restoring refusal with unchanged input and no generated/approved/stopped signals, plus history availability recovery when review starts.
- P2 escaped goal silently dropped: WorkspaceStore now shares a 64 KiB per-record limit across record/load, accommodates the approximately 49 KiB worst-case JSON representation of an accepted 8192-byte goal, bounds history at both 200 entries and 3 MiB, and retains the existing 4 MiB file limit. Oldest-entry eviction and load accounting consistently include JSON array brackets and commas. Added tests exercise 8192 quote bytes and 205 records containing 8191 control bytes plus a nonblank character, preserving goal data through save/load.

No remaining P0-P2 findings in these fixes. Runtime verification is being performed by the root agent; this recheck establishes source-level resolution and verifies relevant test assertions, not a separate test execution.
