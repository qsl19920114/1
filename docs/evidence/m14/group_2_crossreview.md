# Cross-review: shared player/demo

P1 / confidence 9: app/ui/MediaPlayerDialog.cpp:24–28. Fullscreen Close invokes closeEvent -> shutdown -> QDialog::closeEvent -> virtual reject -> exit fullscreen and return; Qt ignores the close while the dialog remains visible. The player remains shut down and cannot play again. Normal close and Escape-then-close are tested; fullscreen-direct-close is not.

Local evidence: Qt 6.11.2 installed QtWidgets framework, nm identifies QDialog::closeEvent at 0x1f46ac and reject at 0x1f429c. Read-only objdump of closeEvent shows the virtual call through offset 0x1c0 at 0x1f4704–0x1f4710. This is code/disassembly review, not a runtime reproduction.

Suggested repair: handle Escape fullscreen restoration inside the shortcut lambda, keep reject/close actual closure, then add focused fullscreen Close cleanup assertion.

## Resolved on final rereview

Root reproduced the defect in docs/evidence/m14/player-close-red.log (closeWhileFullscreenDestroysPlayer failed guard.isNull). Reread fixed source: Escape shortcut alone restores fullscreen; reject always shuts down and calls QDialog::reject. New test closes while fullscreen and checks both dialog and WebEngine page destruction. Inspected player-close-green.log: the new fullscreen-close case passes, but the later fullscreenEscapeAndCloseCleanup case ends in SIGSEGV (0x10 access); the suite did not complete and is not certified green. No remaining cross-review finding. Original finding retained in group_2_crossreview_resolved.jsonl; active group_2_crossreview.jsonl is empty.

Also reread bounded demo stable() helper after setup imports and between initial edits; it waits for three repeated revision/fingerprint refresh results plus confirmed actual preview. Its runtime acceptance remains pending.

## Final verification rereview

Reread final MediaPlayerDialog.cpp/.h and MediaPlayerTest.cpp. keyPressEvent now handles fullscreen Escape before QDialog default rejection, including when the window shortcut is not active; the shortcut retains the same fullscreen behavior. reject and closeEvent always terminate the dialog/player. The Escape test uses QPointer in its eventual assertion, preventing the previous test use-after-delete.

Read actual docs/evidence/m14/player-final.log: all 7 QtTest entries passed, 0 failed, 10536 ms, including closeWhileFullscreenDestroysPlayer and fullscreenEscapeAndCloseCleanup. This supersedes the failed intermediate player-close-green.log. No open cross-review findings remain. No reviewer reruns, builds or edits to source were performed. Full character demonstration remains a separate pending runtime gate.
