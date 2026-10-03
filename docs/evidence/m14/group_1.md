# Group 1 quality review

Reviewed MediaPlayerWidget.{cpp,h}, MediaPlayerDialog.{cpp,h}, MediaPlayerTest.cpp, app/tests CMake integration, and existing MainWindow/HistoryPanel dialog call sites against HEAD b850e8f and approved m14 spec. Read-only source review; no builds, model calls, external requests, or source edits.

## Finding

P1, confidence 9: `app/ui/MediaPlayerDialog.cpp:24–28`: Closing directly from fullscreen first shuts down the player, then QDialog close handling dispatches the overridden reject() which only exits fullscreen. The still-visible window rejects the close event and retains an irreversibly closed player. Move fullscreen Escape handling out of reject() or force actual rejection from closeEvent. Existing test exits fullscreen with Escape before closing and cannot detect this sequence.

Local Qt binary inspection (`otool -tvV /opt/homebrew/opt/qt/lib/QtWidgets.framework/Versions/A/QtWidgets`, QDialog::closeEvent) confirms virtual dispatch during close and an ignore-event write when the object remains visible. Runtime reproduction requested from root; no runtime claim made here.

Other reviewed paths: initial decoded-frame readiness, latched seek/loop readiness, pending JS callback revisions and QPointer lifetime, explicit page-before-profile teardown, finite/clamped command inputs, actual DOM state polling, media error and timeout paths, native seek/volume/rate/loop controls, and CMake linkage. No other concrete P0–P2 defects identified.
