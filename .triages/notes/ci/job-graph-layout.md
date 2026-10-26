---
name: job-graph-layout
description: "Workflows lay out as columns: every build in one, every test in the next; per-lane build/test pairs; ci-success needs only tests and fails a gating test that did not succeed"
updated: 2026-10-26
---

The maintainer wants the run's job graph in clean columns (2026-10-26). In `pr.yaml`: changes/lint/docs, then
cppcheck/version/security/Analysis (CodeQL + Sonar in one box)/claude review, then **every build**, then **every
test**, then CI Success. `post_merge.yaml` follows the same rule for its builds and tests. Analysis needs only
`changes`, not cppcheck: in the build column GitHub drew it among the builds (middle, then top, depending on
definition and `needs` order, which is undocumented), breaking the build-to-test row alignment.

How it is built:
- `_build.yaml` (one job) builds, stages, and plans the test matrix (`plan-tests.sh`). It outputs everything a test
  needs, so a test call is just `build: ${{ toJSON(needs.<lane>-build.outputs) }}` into `_test.yaml` (one matrix
  job), `_cross_codegen.yaml` (emit only) or `_coverage.yaml` (post_merge only).
- One explicit `<lane>-build` and `<lane>-test` job per lane in the caller, with matching `name`s. The shared
  `needs`/`if`/`permissions` are YAML anchors (`&build-gate`, `*read`); GitHub, actionlint 1.7.12, zizmor and
  `classify-changes.py check` all accept them. Merge keys (`<<:`) are not supported by GitHub, so a whole `with:`
  cannot be shared.
- `_build.yaml` defaults are the common lane: `modes: rel`, `shard-count: 1`, `pack: true`. Linux GCC passes
  `rel dev`, 2 shards, `pack: false`; the nightly sanitizers lane 4 shards.
- `ci-success` needs only the gating test jobs, not their builds. `needs` is not transitive and a skipped need
  counts as a pass, so a failed build (which skips its tests) would slip through; its second step fails when
  `needs.changes.outputs.compile == 'true'` and any gating test's result is not `success`. A new gating lane goes in
  both its `needs` and that condition. `collect-release` needs only test jobs too (a skipped test skips it), and
  `_test.yaml` passes the build's outputs through (`build`, `artifact`, digests) for it and `_coverage.yaml`.

**Why:** a matrix of reusable workflow calls draws one group per cell with all of that call's jobs (build, test,
coverage) inside, so builds and tests of different lanes landed in different columns. A matrix also makes every
test wait on every build.

**How to apply:** a new lane is a `<lane>-build` + `<lane>-test` pair, not a matrix entry; a new analysis that
builds the project goes into `_analysis.yaml` behind an input. Related: [[gha-parallel-steps-limits]],
[[ci-name-casing]].
