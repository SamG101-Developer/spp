---
name: job-graph-layout
description: "One box per lane in the job graph: each lane is one call to _lane.yaml (or _coverage/_cross_codegen), build then tests inside; callers need the lane job"
updated: 2026-10-27
---

The maintainer wants each platform's build and tests grouped in one box of the run's job graph (2026-10-27; this
replaced a build column + test column layout from the day before). A lane is one job in the caller that calls a
reusable workflow holding the whole lane:
- `_lane.yaml`: `_build.yaml` then `_test.yaml` (`build: ${{ toJSON(needs.build.outputs) }}`). Its inputs are both
  workflows' inputs, passed through with the same defaults (`modes: rel`, `shard-count: 1`, `pack: true`); a new
  `_build.yaml` or `_test.yaml` input has to be added here too. Outputs: the build's `artifact` and digests.
- `_coverage.yaml` (post_merge): build, tests, then a report job that waits on every test cell to merge their
  profiles, so it cannot live in a build/test pair. Output: the report's artifact name, for Sonar.
- `_cross_codegen.yaml` (pr): the all-targets build, then the emit matrix (which also runs the s++ conformance).
- pr.yaml's lane `if`/`needs` are YAML anchors (`&lane-gate`, `&lane-needs`, `*read`); GitHub, actionlint 1.7.12,
  zizmor and `classify-changes.py check` all accept them. Merge keys (`<<:`) are not supported by GitHub.

A call job's result is `failure` if any job inside failed, so a failed build fails its lane instead of skipping its
tests. `ci-success` therefore needs just the gating lanes (the 4 Linux ones), with no extra "did the tests run" step;
`collect-release` needs the release lanes and reads their outputs. post_merge's Sonar waits on the Linux GCC lane (to
restore that run's tree) and runs unless the lane's build did not stage: `result == 'skipped' || outputs.artifact != ''`,
so a test failure alone does not skip it. `continue-on-error` is not allowed on a call job; advisory lanes pass
`advisory: true`, which the jobs inside apply.

**Why:** a matrix of workflow calls (the first `_lane.yaml`) laid each cell out on its own, so lanes with different inner
jobs drew build and test at different depths, and anything after the matrix waited on every cell. Explicit per-lane
calls avoid both. Not yet confirmed on GitHub: how the nested calls draw (one box or boxes inside a box) and the check
names they produce (e.g. `Linux GCC / Build / Build`).

**How to apply:** a new lane is one job calling `_lane.yaml`, not a matrix entry and not a separate build and test job;
a gating one also goes in `ci-success`'s `needs`. A lane with a step after its tests gets its own workflow like
`_coverage.yaml`. Related: [[gha-parallel-steps-limits]], [[ci-name-casing]].
