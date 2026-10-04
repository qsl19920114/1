# Component navigation and image framing implementation plan

> Execute with superpowers:subagent-driven-development, independent specification then quality review. Current dedicated qt-video-workbench branch is clean at cddd6c2. User authorizes continuing improvements, test/release and commit/push; do not repeat approval loops.

**Goal:** Make components easy to locate and operate, and expose actual original-template image framing controls to both native Qt and the existing approved Agent flow.

**Architecture:** Extract a reusable ComponentBrowser from MainWindow's tree construction/search/navigation. Retain the exact compiled entity identity and existing production edit/approval guards. Add backward-compatible image-fit (cover/contain) and image-position-x/y (0–100 percent) to original title-card/story-reel Author Packages; expose only real implemented inspector bindings. Existing projects keep copied packages; new projects get updated templates. No Hypit modifications or paid providers.

## T060 component browser

Files: app/ui/ComponentBrowser.{h,cpp}; MainWindow.{h,cpp}; tests/unit/ComponentBrowserTest.cpp, ProductExperienceTest.cpp; app/CMakeLists.txt and tests/CMakeLists.txt coordinated by root.
- [x] Search component/track labels and exact IDs, option all/editable, count visible/total and clear-filter action. Tree objectName `components` and role indices preserve existing integrations.
- [x] Browser owns tree creation and snapshot rebuild. Preserve selected ID on refresh when visible, clear if hidden/no match; never keep actionable hidden selection. Snapshot refresh/filter must not seek or emit edits. Explicit click/Enter activation emits exact ID/start frame; arrow keys navigate visible children.
- [x] MainWindow calls browser.setSnapshot and uses browser.tree() for existing property/asset guards. Component activation routes existing selectScene; selecting scene hidden by filter reveals that requested component by resetting filter and then selects/seeks once. Project switches clear search/filter; ordinary snapshot refresh preserves filters. Expose accessible search/count/no-result hint.
- [x] Meaningful RED/GREEN: search+editable+selection refresh/hidden+Enter activation/no unintended edits; existing SceneStrip and Agent selection semantics continue.

## T061 actual image framing controls

Files: templates/title-card and templates/story-reel original packages/render.js/activation.js/main.svml/template.json; tests/template/*.test.mjs; docs/TEMPLATE_CONTRACT.md.
- [x] Add default `imageFit='cover'`, `imagePositionX=50`, `imagePositionY=50` options. Validate fit enum and finite 0–100 values. IR image style uses object-fit and object-position. Missing attributes preserve current rendering; supplied new attrs must be parsed strictly and rejected if invalid.
- [x] Add literal attributes image-fit/image-position-x/image-position-y with optional backwards defaults. Register actual editable select/number inspector fields using pinned local Hypit Companion contract, with two labeled fit options. Default source templates explicitly include attrs so writable fields exist in fresh snapshots. Update template manifest version to1.1.0; keep module major1 for compatibility.
- [x] RED/GREEN Node tests for default compatibility, contain vs cover, focal positions, bounds/nonfinite/injection rejection. Existing tests pass. Record exact upstream contract references.

## T062 real Qt / Hypit verification

Files: tests/integration/ComponentFramingE2ETest.cpp; tests/CMakeLists.txt (root); ignored .workbench/deliverables-m15; docs/evidence/m15.
- [x] Use real production MainWindow + Agent bridge + controllers, create fresh title-card project, import existing attributed portrait. Browser search/Enter chooses exact component without unauthorized writes. Current Codex request changes only real fit/position fields after review/approval; confirm before/after source, actual inspector and preview version.
- [x] Export baseline cover and adjusted contain films, verify build/get/media full decode; inspect actual output frames for visible framing difference. Exercise invalid range -> compile rejection/rollback without claiming success and retain last valid output. Do not re-record an entire walkthrough unless needed; user asks improvements, not another mandatory video.
- [x] Existing model adapter/approval protections retained; no pixel upload, no external generation calls. Record PASS JSON with source/media fingerprints and stopped owned Studio.

## T063 release1.7

- [x] Independent spec/quality review and concrete fixes; complete build and full CTest plus real integration (separate opt-in model call).
- [x] Version1.7.0 package, relocated native startup and strict signature/archive/license checks. Update STATUS/tasks/USER_GUIDE/TEST_REPORT.
- [x] Commit/push same branch, matching source archive, new launcher opening current verified project. Preserve earlier reports/media/packages.

Verification: explicit Qt prefix /opt/homebrew/opt/qt, coordinated single debug build; `ctest --test-dir build --output-on-failure`; opt-in `build/tests/component_framing_e2e_test`. No screenshots acceptance; output frames only verify real rendering.

Final: full CTest38/38 (208.57s); real model driver3/3 (61.689s),1request/approval in final run; template16/16; real generatedAgentstory9fields; independentreviewclosed2P1; native1.7relocation/signature/ZIP/licenses PASS. Runtime integer-string issue corrected with explicit valueType, not validation coercion. Source range rejection and actual guarded source rollback tested separately. See docs/evidence/m15.
