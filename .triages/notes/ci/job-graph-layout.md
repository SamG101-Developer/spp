---
name: job-graph-layout
description: "GitHub flattens called workflows into the caller's graph, columns by needs depth; one lane call per platform (build then tests, one row); ci-success needs lanes"
updated: 2026-10-27
---

The run's job graph does not draw a box around a reusable workflow call: it flattens the called jobs into the
caller's graph, named `<caller job> / <job>`, and puts each job one column right of what it `needs` (seen
2026-10-27). Jobs with the same upstream and downstream are stacked into one card; a matrix job is one "Matrix:"
card, "Waiting for pending jobs" until its matrix is known. So a lane's build and tests are always in adjacent
columns, one row per lane; only a single job (build then tests) would make one card, which the maintainer declined
(it means two lane shapes, as multi-cell lanes need the matrix). Inlining a lane's jobs instead of calling workflows
draws the same, so the lanes call the shared workflows:
- `_build.yaml` (one job) builds, stages and plans the test matrix (`plan-tests.sh`); its outputs are all a test job
  needs, handed on as `build: ${{ toJSON(needs.build.outputs) }}`. `_test.yaml` is that matrix job.
- `_lane.yaml`: `_build.yaml` then `_test.yaml`, inputs passed through with the same defaults (`modes: rel`,
  `shard-count: 1`, `pack: true`); a new input to either has to be added here too. Outputs: the build's `artifact`
  and digests.
- `_coverage.yaml` (post_merge): `_build.yaml`, `_test.yaml`, then an inline report job that waits on every cell to
  merge their profiles. Outputs `coverage-report` (empty when it did not upload) for Sonar.
- `_cross_codegen.yaml` (pr): `_build.yaml` (all targets), then the inline emit matrix (which also runs the s++
  conformance).
- pr.yaml's lane `if`/`needs` are YAML anchors (`&lane-gate`, `&lane-needs`, `*read`); GitHub, actionlint 1.7.12,
  zizmor and `classify-changes.py check` all accept them. Merge keys (`<<:`) are not supported by GitHub.

A call job's result is `failure` if any job inside failed, so a failed build fails its lane instead of skipping its
tests. `ci-success` therefore needs just the gating lanes (the 4 Linux ones), with no extra "did the tests run" step;
`collect-release` needs the release lanes and reads their outputs. post_merge's Sonar waits on the Linux GCC lane (to
restore that run's tree) and runs unless the lane's build did not stage: `result == 'skipped' || outputs.artifact != ''`,
so a test failure alone does not skip it. `continue-on-error` is not allowed on a call job; advisory lanes pass
`advisory: true`, which the jobs inside apply.

**Why:** the maintainer wanted each platform's build and tests grouped (2026-10-27); a row per lane is the closest the
graph allows. A matrix of workflow calls made anything after the matrix wait on every cell. Not yet confirmed on
GitHub: the Sonar condition reading a lane's output after its tests failed.

**How to apply:** a new lane is one job calling `_lane.yaml`, not a matrix entry; a gating one also goes in
`ci-success`'s `needs`. A lane with other jobs gets its own workflow like `_coverage.yaml`, calling `_build.yaml` and
`_test.yaml`. Related: [[gha-parallel-steps-limits]], [[ci-name-casing]].
