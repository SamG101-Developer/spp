---
name: readme-test-badges-after-suite
description: "After a full unit-test run with no regressions, update README.md's two test badges (total, passed, %)"
updated: 2026-10-09
---

After a full gtest suite run that doesn't regress (nothing new failing vs the known-failure list), update the two
shields.io badges on README.md lines 3-4 with that run's numbers:

- `badge/Tests-<total>-green` — total = passed + failed (e.g. 2701)
- `badge/Test%20Passing-<pct>%25%20%28<passed>/<total>%29-cactus` — pct to one decimal (2683/2701 → `99.3`)

**Why:** the maintainer asked for this 2026-10-09 so the README tracks the suite without a separate pass.

**How to apply:** only on a full run (not a filtered subset) and only when there's no regression; keep the URL
encoding (`%25` = %, `%28`/`%29` = parens). Mention the badge update in the run report. Related: `batch-tests-at-end`,
[[build-workflow]].
