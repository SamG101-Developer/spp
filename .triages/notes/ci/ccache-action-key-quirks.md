---
name: ccache-action-key-quirks
description: "hendrikmuhs/ccache-action v1.2.24 append-timestamp:false still appends \\\"-\\\"; cache keys end in an ISO timestamp (3 dash segments)"
updated: 2026-10-21
---

CI's compiler cache is hendrikmuhs/ccache-action (setup-toolchain, since 2026-10-21), with `install: "no"` over a pinned binary ([[ccache-stale-clang-modules]]).

- `append-timestamp: false` is broken in v1.2.24: restore.ts tests the input as a string, so "false" is truthy and a trailing `-` is appended to the key anyway. Keep the default (timestamp) and give no sha in the key.
- Saved keys are `<ccache|sccache>-<key>-<ISO date>`; the date splits into 3 dash segments, so prune-caches.sh groups `ccache-`/`sccache-` with `:3`.
- Default cache dir is `$GITHUB_WORKSPACE/.ccache` (inside the checkout); compiler-cache.sh exports CCACHE_DIR/SCCACHE_DIR under RUNNER_TEMP first.

**How to apply:** on a ccache-action bump, re-check restore.ts for the string-truthiness bug before switching to `append-timestamp: false`, and keep the prune segment count in step with the key shape. See `ci-pipeline-refactor-plan`.
