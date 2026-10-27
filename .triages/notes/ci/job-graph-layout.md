---
name: job-graph-layout
description: "One flat box per lane in the job graph: each lane is one call to a lane workflow whose build and test jobs are inline, not nested workflow calls"
updated: 2026-10-27
---

The maintainer wants each platform's build and tests grouped in one box of the run's job graph (2026-10-27; this
replaced a build column + test column layout). A lane is one job in the caller that calls a lane workflow, and the
lane workflow's jobs are written inline: a job that itself calls a reusable workflow draws as a box of its own, so a
lane built from `_build.yaml` + `_test.yaml` calls drew boxes inside the box. Those two workflows are gone.
- `_lane.yaml`: build job, then the test-matrix job. The caller passes the build and test inputs (defaults
  `modes: rel`, `shard-count: 1`, `pack: true`); outputs the build's `artifact` and digests.
- `_coverage.yaml` (post_merge): build, a test job without valgrind that saves a profile per cell, then a report job
  that waits on every cell to merge them. Outputs the report's artifact name (`coverage-report`) for Sonar.
- `_cross_codegen.yaml` (pr): the all-targets build, then the emit matrix (which also runs the s++ conformance).
- The build job's steps after checkout are the `build-and-plan` composite action (`plan-tests.sh`, then `compile`),
  shared by all three. The test job cannot be shared that way, since composite actions cannot hold its parallel
  group ([[gha-parallel-steps-limits]]), so `_lane.yaml` and `_coverage.yaml` each spell it out; a change to one
  usually belongs in the other.
- pr.yaml's lane `if`/`needs` are YAML anchors (`&lane-gate`, `&lane-needs`, `*read`); GitHub, actionlint 1.7.12,
  zizmor and `classify-changes.py check` all accept them. Merge keys (`<<:`) are not supported by GitHub.

A call job's result is `failure` if any job inside failed, so a failed build fails its lane instead of skipping its
tests. `ci-success` therefore needs just the gating lanes (the 4 Linux ones), with no extra "did the tests run" step;
`collect-release` needs the release lanes and reads their outputs. post_merge's Sonar waits on the Linux GCC lane (to
restore that run's tree) and runs unless the lane's build did not stage: `result == 'skipped' || outputs.artifact != ''`,
so a test failure alone does not skip it. `continue-on-error` is not allowed on a call job; advisory lanes pass
`advisory: true`, which the jobs inside apply.

**Why:** a matrix of workflow calls laid each cell out on its own, so lanes with different inner jobs drew build and
test at different depths, and anything after the matrix waited on every cell; nested calls drew nested boxes.
Not yet confirmed on GitHub: the flat box, and the Sonar condition reading a lane's output after its tests failed.

**How to apply:** a new lane is one job calling `_lane.yaml`, not a matrix entry; a gating one also goes in
`ci-success`'s `needs`. A lane with other jobs gets its own workflow like `_coverage.yaml`, with inline jobs and
`build-and-plan`. Related: [[ci-name-casing]].
