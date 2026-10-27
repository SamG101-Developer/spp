---
name: gha-parallel-steps-limits
description: "GH Actions parallel steps (June 2026) cannot be used inside composite actions, so the spp build matrix can't use them; actionlint needs an ignore"
updated: 2026-10-26
---

GitHub shipped step-level concurrency on 2026-06-25 (`background: true`, `wait`/`wait-all`,
`cancel`, `parallel:`). Three things bound how much of it spp's CI can use:

1. **Not inside composite actions.** workflow-syntax.md states it for both `background` and
   `parallel`. Since the build was consolidated into `.github/actions/compile/action.yaml`
   (commit c91deadf), the main build/test path cannot use it at all. A composite action *can*
   itself be a background step, so the only route would be splitting `compile` back into several
   actions called from each job — which re-duplicates the caller boilerplate across the 4 call
   sites. Not worth it.
2. **It never shortens the matrix.** Parallel steps run on one runner; the matrix parallelism is
   across machines. `build-test-extra` / `release-build` span 5 runner images, so they cannot
   collapse. See [[ci-runner-core-topology]] — collapsing the 4 same-image `build-test` cells onto
   one 2-physical-core box would be ~4x slower and OOM.
3. **actionlint v1.7.12 rejects the keys** (rhysd/actionlint#693). `.github/actionlint.yaml`
   suppresses it, scoped per-file. Delete those entries when the pre-commit rev is bumped.
   SchemaStore (`check-github-workflows`) and zizmor already accept them.

Applied in `_docs.yaml` (doxygen install ∥ uv sync), `_security.yaml` (the two OSV scans) and `_test.yaml`'s job (2026-10-21: every suite of a cell in one group; `pack`, on by default in `_build.yaml`, puts stl beside gtest on 1-shard lanes). A composite (`run-tests`) works as a member. actionlint also misses step ids set inside a group (`steps.tests`), ignored per file.
Rejected in `_cross_codegen.yaml`: `compose-key.sh` fingerprints `$CXX --version`, so the cache
key depends on `Set up toolchain` having already run — overlapping them silently poisons the key.
