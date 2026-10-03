# Qt character material showcase and player implementation plan

> Execute with superpowers:subagent-driven-development, independent spec and quality reviews. Continue existing dedicated qt-video-workbench branch; user authorized ongoing iteration and real videos. Higher-priority autonomy instructions permit execution without repeated design approval.

**Goal:** Make Qt's embedded AI material-edit workflow demonstrable through real character material variants, and replace the minimal video dialog with reusable native playback controls.

**Architecture:** Keep the production AgentWorkbenchBridge/approval/editor/export chain. Add an explicit character-material goal entry, which only prepares a scoped request. Extract reusable MediaPlayerWidget owning the WebEngine media surface and Qt controls; MediaPlayerDialog owns presentation/lifetime. Existing preview transport remains separately owned by Studio. Official swap-host needs external generative providers; the user explicitly confirmed local material substitution preserving layout/animation, with accurate attribution, not unverified video identity regeneration.

**Tech:** Qt6 Widgets/WebEngine, C++17, existing Codex login, pinned Hypit0.2.10, local FFmpeg, Python scripts.

## T056 reusable native media player

Files: app/ui/MediaPlayerWidget.{h,cpp}, MediaPlayerDialog.{h,cpp}, app/CMakeLists.txt; tests/unit/MediaPlayerTest.cpp, tests/CMakeLists.txt.
- [x] RED meaningful state/boundary tests; load real local video in WebEngine for ready/play/seek/rate/volume/loop/error and cleanup.
- [x] Add QWidget owning isolated WebEngine profile/page and native controls: play/pause, stop/restart, ±5s, seek slider, elapsed/duration, volume/mute,0.5/1/1.5/2x, loop, fullscreen request. Disable unavailable controls until actual media ready; show actual load/decode errors. Use monotonic local lifetime-safe JS callbacks; no fabricated readiness or user input inserted into executable JS.
- [x] MediaPlayerDialog wraps widget, handles fullscreen/Escape and closes media. Preserve existing constructor and MainWindow call sites.
- [x] Expose stable object names and current observed playback state for tests/demo; retain video DOM element for existing integration checks. Build and targeted CTest.

## T057 character-material AI entry

Files: app/ui/MainWindow.{h,cpp}, AgentPanel.{h,cpp}; tests/unit/AgentPanelTest.cpp, ProductExperienceTest.cpp.
- [x] Add `composeCharacterGoal(name,binding)` using exact registered asset path, current component scope, preserve other fields. Require existing valid selection/binding and revoke pending review. No automatic generate/edit/approve.
- [x] Add `handoffCharacterToAgent` action next to existing handoff; same visible single compatible material/busy/restoring gates. Say “人物素材替换” and preserve honest scope; user identifies the material, no visual recognition claim.
- [x] Show concise selected component name in task scope; retain full ID as tooltip for diagnostics. Verify scoped request and no writes with tests.

## T058 real variant showcase

Files: scripts/showcase/prepare_character_materials.py; tests/integration/CharacterShowcaseDemo.cpp; scripts/showcase/build_character_showcase.py; tests/CMakeLists.txt; docs/CHARACTER_SHOWCASE.md.
- [x] Read pinned Hypit swap-host source/runtime and record provider boundary. Extract only explicit bounded regular owner-supplied portrait assets from existing official archive, preserve URL/member/hash/provenance; no upstream writes or paid calls. Inspect actual portraits before use.
- [x] Create a title-card project with real imported portrait. Export baseline, use production character action/current Codex/review/approve to replace portrait, export variant; repeat another distinct portrait if source assets support3variants. Validate source unchanged beforeapproval, imagefieldchanged and otherfieldsunchanged, actual preview identity, outputfull decode.
- [x] Record real own Qt window with honest time intervals, include upgraded player controls via actual operations. Deliver continuous walkthrough plus at least baseline/changed playable films and clearly labelled comparisons. Do not label official upstream generated videos as newly generated here.
- [x] Build a local relative-link showcase index with sources, limitations, actual results. Keep old reports/deliveries. No screenshot acceptance.

## T059 verification and 1.6 release

- [x] Independent spec/quality review, fix concrete defects. Full CTest after final product changes; actual opt-in character/player demonstration with current model.
- [x] Package1.6.0, native relocated startup and archive/signature integrity; update STATUS/tasks/USER_GUIDE/TEST_REPORT and evidence.
- [x] Commit/push current branch, matching source archive and local launcher. Final response links to usable videos/showcase/application.

Commands: `cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt`; `cmake --build build -j 6`; `ctest --test-dir build --output-on-failure`; opt-in `build/tests/character_showcase_demo`. Agent/model export tests are not hidden behind fixture passes.
