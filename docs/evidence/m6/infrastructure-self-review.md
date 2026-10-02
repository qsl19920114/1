# M6 infrastructure self review

Production and test edits frozen at 2026-10-02 12:33:44 Asia/Shanghai after fixing the first quality review findings. No commit or upstream edit was made by this worker.

## APIs and ownership

- `RuntimePaths::resourceRoot()`, `templateDirectory()`: executable-relative Resources for macOS bundles, executable/resources for other layouts. No source tree or CWD template fallback.
- `RuntimePaths::processEnvironment(explicitToolPaths)`, `resolveTool(name, environment)`: child-only PATH. Explicit parents precede six standard executable directories; inherited absolute entries follow. No shell/global changes.
- `AppConfig`: resolved nodePath, ffmpegPath, ffprobePath and processEnvironment, with nodeExplicit distinguishing a tools.node declaration from automatic discovery. Regular non-symlink JSON <= 64 KiB; required Hypit string types/lengths and optional explicit executable paths validated. Missing unspecified tools do not block configuration loading.
- `JsonProcess`, `MediaValidation`, `StudioProcess`: environment setters. Studio also accepts setNodePath(path, explicitlyConfigured); ProjectController constructor/configure propagates both. Probe, Studio and Export use one RuntimePaths::hypitInvocation strategy. .mjs/.js use the resolved absolute Node, while shell/other executable launchers retain direct execution with the child environment when Node was discovered automatically. An explicitly configured Node may bypass only the full SHA256-matched fixed official shell wrapper to its actual bin/hypit.mjs entry; custom non-JS wrappers return a clear limited-support error instead of being bypassed. Empty mock configuration behavior is retained.
- `ExportController::clearFinishedCache()` and `cacheCleared()`: asynchronous terminal verification; existing export/cancel/recovery state machine kept. ExportWorkspace exposes safe validation/removal helpers used after worker shutdown.

## Logger concurrency

The same QLockFile covers constructor probing and the complete size check, rotation and append. Each lock attempt waits at most 100 ms. Temporary contention reports a warning and lastError, skips that record and keeps the writer available; a later successful append clears the transient error. A constructor blocked by a held lock can create its first file later, and a writer can recover from a dead process that left a stale lock and renamed current log without recreating it. Existing bounded-record and unsafe-path failures remain enforced.

## Cache safety decisions

Cleanup requires idle controller, inactive complete/failed/cancelled phase, known Build ID and matching configured Hypit version. It re-reads the exact persisted task and validates the UUID directory, marker, all frozen input hashes, canonical containment and every generated cache entry without symbolic links or special files. Each subsequent command revalidates the record and tree.

The exact Build must report work.done and a matching terminal result/outcome, without attention. Runtime activity must be complete and contain no unknown, active or other Build. The implementation conservatively refuses other terminal Builds too. Only this frozen profile is passed to runtime down; a stopped JSON response with exit 0 is mandatory before revalidation and removal of its UUID directory and current task record. Project source/assets, delivered output and sibling Build directories are preserved. Empty Build IDs and unsubmitted caches are refused.

Fixed upstream contracts checked locally: packages/cli/src/commands/execution.ts:225-311 (status and failure exit), commands/execution.ts:168-195 (activity), commands/environment.ts:300-310 (runtime-down stopped response), arguments.ts:328-343 (workspace/runtime flags). The worker command leaves managed external Programs unchanged as documented by that source.

## Verification

- infrastructure-red.log: 14 intended behavior failures against original config/JSON process/logger behavior.
- infrastructure-green.log: initial config/PATH/log fixes pass.
- process-environment-red.log / green.log: Studio and media decode PATH behavior fails then passes.
- export-cache-red.log / green.log: 23 original cleanup/record safety cases fail then pass.
- explicit-node-red.log / green.log: Probe, ProjectController to Studio, and cleanup CLI honor a differently named absolute Node executable only after the fix.
- infrastructure-regression-green.log: 10/10 suites, including existing M4 export workspace/controller/media and Studio process cases.
- infrastructure-final-green.log: 5/5 suites freshly rerun after explicit Node changes, 95.94 seconds.
- log-concurrency-red.log / green.log: three cross-process initialization/rotation/crash cases reproduce the permanent-stop defect, then the full 10-case logger suite passes. Three writing subprocesses produce 300 distinct valid records while rotating a full log; test writers explicitly retry reported contention.
- shell-launcher-red.log / green.log: a loaded executable shell reproduces Node SyntaxError, then generic shell Probe/Studio and other-executable Export paths pass. The real fixed upstream shell is copied only into a temporary runtime fixture and tested with both default Node discovery/direct shell execution and an explicitly configured differently named Node/verified bin entry. Custom shell replacement is rejected. No upstream script is committed or vendored.
- quality-infra-green.log: 3/3 current config/launcher, logger and cache suites pass after both review fixes.
- git diff --check: clean. Owned production files have no hardcoded /Users path or template compile-source fallback.

Temporary test harness is /private/tmp/qvw-m6-infra-src and canonical build /private/tmp/qvw-m6-infra-build-canonical; root full application/release/real integration checks are separate. Cache tests are subprocess fixtures; real cleanup acceptance belongs to root's recorded demo. No broad historical cache garbage collection is implemented.
