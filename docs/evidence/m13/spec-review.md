# T052 / T053 specification review

Date: 2026-10-03. Result: **PASS within the static review scope below; no concrete P0–P2 findings.**

Reviewed the working tree against `08b6f5ef093ef314a1917438a13796191a0f70b2`, including untracked `app/workflow/` and `scripts/showcase/prepare_hypit_samples.py`. Requirements: `AGENTS.md` and `docs/plans/2026-10-03-agent-workbench-showcase.md`. Review groups were T052 UI/controller wiring and T053 catalog/preparation. No source changes, builds, model requests, network downloads, or rendering were performed by this reviewer.

## T052: PASS

- `MainWindow.cpp:158` prepares the actual selected registered asset's name/binding, then opens the Agent tab. The handler does not emit edit, generate, or approve. It uses selected items rather than a possibly different current item.
- `MainWindow.cpp:245` and `compatibleAssetField()` require one non-hidden selected asset, a selected compiled component, and an editable field with compatible image/video binding. Backend/editor readiness, editing/import/model activity, export activity and Agent restoration disable handoff.
- `AgentPanel.cpp:110` / `composeAssetGoal()` require a selected entity and valid local asset binding, reuse `composeGoal()` to clear pending review, and set the next request to the selected component. Review cancellation uses the production controller's stop path. It does not generate a model request. `AgentController::setScope()` configures a future request, preserving the approved scope of existing tasks.
- `AgentPanel::setProjectContext()` uses actual project name, registered asset count and loaded snapshot revision. `showDocument`, `showSnapshot`, and `clearDocument` refresh it; project switching initially uses an empty snapshot. Labels use plain text. No authentication success status was added.
- `workflow/AgentWorkbenchBridge.cpp` preserves the previous main connections, including update-plan/content equality/canApprove before approve. UI error logging moves into the bridge; main retains file logging. Both `main.cpp` and the developing demo call this bridge. CMake adds an application-level `qvw_workflow -> qvw_ui + qvw_agent` dependency without reversing lower-layer dependencies.
- Read `AgentPanelTest` and `ProductExperienceTest` additions for no-execution signals, selection/current-item mismatch, multiple/hidden selections, restoration/thinking, scope and approval revocation. Existing `handoff-green.log` records both test executables passing; this reviewer did not rerun them.

Contract check: the compatibility path uses `InspectorField::isEditable()` and actual field binding, while the production Agent context supplies real `entityId`/`fieldId` and registered `bindingValue`. `ProposalService::validate()` independently checks compatible registered bindings. No invented Studio API or writable field was introduced.

## T053: PASS

- Optional `samples.catalogPath` is validated and resolved from the repository root. Both startup and config reload pass it to `SampleCatalog::discover`; absent config preserves fallback discovery. Discovery makes no network calls.
- Catalog bounds: 256 KiB JSON, 32 entries, 64 MiB/video and 512 MiB total catalog hash input. Paths reject absolute/empty/dot/parent components, backslashes, NUL, missing/nonregular files and symlinks in relative path components. Canonical paths must remain under the catalog directory. Size, lowercase SHA-256, duration, dimensions, name and HTTPS provenance are validated. Hash duplicates merge provenance across catalog/fallback sources; invalid catalog data does not hide valid fallback clips.
- Preparation reads the two URLs from the pinned upstream README, downloads via HTTPS with byte/time bounds, inventories archive entries, extracts only the three selected regular video members, and refuses unsafe paths/selected links. It does not run upstream instructions or invoke render providers. Full FFmpeg decode precedes verified catalog creation; failures are recorded in the manifest.
- Local evidence inspection verified catalog and manifest sample arrays match, all four current files have their declared size/hash, and all four hashes differ. The manifest records FFprobe and full-decode exit code 0 with empty decode stderr for every sample. Decode was not repeated by this reviewer.

| Clip | Duration | Dimensions | Bytes | SHA-256 prefix |
|---|---:|---:|---:|---|
| Published final film | 137.066667 s | 1080×1920 | 43,912,844 | `2a8b10d88656` |
| Interview | 7.033333 s | 720×1280 | 7,610,499 | `71c8ec2efd05` |
| Ranking | 10.033333 s | 720×1280 | 4,963,242 | `7356166e9ac7` |
| Podcast | 9.033333 s | 720×1280 | 7,729,281 | `fd2afbacba90` |

Evidence: ignored `.workbench/showcase/catalog.json`, `preparation-manifest.json` and `archive-inventory.json`. Inventory contains 211 members; all three selected entries are regular files. The original inventory records three local files with one unique SHA-256 (`e63183cb8638…`). The upstream checkout is `1af179d3f58284c2d6d3c1f63052172a4fe1b5a6`; the current README hash matches the manifest. README lines 10/12 supply final/archive URLs; `productions/explainer/authors/assets.svml:46` onward identifies the Hypit examples separately from competitor/reference media.

The 7.03-second interview remains playable but cannot create the 8-second video-story template. `MainWindow::setSamples()` and `useSelectedSample()` enforce this, with `shortOfficialSampleIsPreviewOnly()` coverage. Catalog unit additions cover config typing, source/hash deduplication, invalid paths/symlinks/hash/metadata and malformed/oversized JSON.

## Scope limits

This PASS concerns T052/T053 implementation and the inspected evidence only. It does not certify the ongoing T054 real-model walkthrough/report, full CTest results, packaging, relocation, release version, commit or push. Those gates remain owned by the main task. Unit fixtures establish behavior; they are not evidence of real-model execution. Broader runtime acceptance requires the main task's current build/test/demo evidence.
