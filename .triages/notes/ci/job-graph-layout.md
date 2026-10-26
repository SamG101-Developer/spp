---
name: job-graph-layout
description: "Workflows lay out as columns: every build in one, every test in the next; per-lane build/test pairs, tests fail on a failed build so ci-success needs only tests"
updated: 2026-10-26
---

The maintainer wants the run's job graph in clean columns (2026-10-26). In `pr.yaml`: changes/lint/docs, then
cppcheck/version/security/claude review, then **every build** plus Analysis (CodeQL + Sonar in one box), then **every
test**, then CI Success. `post_merge.yaml` follows the same rule for its builds and tests.

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
- Every test job has `if: *test-gate` (`!cancelled() && !contains(needs.*.result, 'skipped')`): it runs after a
  failed build too, and its first step fails when the build staged no artifact. So `ci-success` and
  `collect-release` need only the test jobs (a skipped need counts as a pass, and `needs` is not transitive); a
  skipped build (compile gate off) still skips its test. `_test.yaml` passes the build's outputs through (`build`,
  `artifact`, digests) for `collect-release` and `_coverage.yaml`.

**Why:** a matrix of reusable workflow calls draws one group per cell with all of that call's jobs (build, test,
coverage) inside, so builds and tests of different lanes landed in different columns. A matrix also makes every
test wait on every build.

**How to apply:** a new lane is a `<lane>-build` + `<lane>-test` pair, not a matrix entry; a new analysis that
builds the project goes into `_analysis.yaml` behind an input. Related: [[gha-parallel-steps-limits]],
[[ci-name-casing]].
