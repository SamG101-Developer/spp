---
name: reusable-workflow-caller-grants-every-job
description: "A reusable-workflow caller must grant the permissions of every job in it, even ones its inputs skip; else the whole workflow is invalid at load, and only that event shows it"
updated: 2026-11-11
---

GitHub checks a reusable-workflow call against the permissions of **every** job in the called file, before any `if:`
runs. `post_merge.yaml`'s `sonar` job called `_analysis.yaml` with `sonar: true` only, under `permissions: *read`, and
`_analysis.yaml`'s `codeql-cpp` and `codeql-python` jobs (skipped by their inputs) ask for `security-events: write` and
`actions: read`. The whole post-merge workflow was then invalid ("The nested job 'codeql-cpp' is requesting 'actions:
read, security-events: write', but is only allowed 'actions: none, security-events: none'"), so nothing in it ran after
the merge of #135 (2026-11-11). Fixed by granting the caller the same three permissions as `pr.yaml`'s `analysis` job.

**Why:** the error only appears for the event that loads that workflow, so PR CI cannot catch a post-merge mistake;
it shows on master after merging.

**How to apply:** a job that calls `_analysis.yaml` (or any reusable workflow) takes the union of its jobs'
permissions. Adding a permission to a called job means adding it to every caller, in `pr.yaml` and `post_merge.yaml`.
