---
name: python-codeql-alerts-frozen
description: "Old py/* CodeQL alerts come from the removed default setup (last scan July 2026); the codeql-python job ignores tests, so it may never refresh the gtest-parallel ones"
updated: 2026-11-01
---
The open `py/*` CodeQL alerts (`docs/doxygen_filter.py`, `tests/gtest-parallel/gtest_parallel.py`) were last updated on 2026-07-27 by GitHub's default setup (`dynamic/github-code-scanning/codeql:analyze`, category `/language:python`). Default setup is now `not-configured`, and from then until the `codeql-python` job in `_analysis.yaml` was added no Python file was scanned by CodeQL.

**Why:** that job is a different analysis under the same `/language:python` category, and `.github/codeql-config.yaml` ignores `tests`, so it does not scan `tests/gtest-parallel`. Editing that file cannot close its alerts.

**How to apply:** after the job's first run on `master`, dismiss any `tests/gtest-parallel` alert still open, since it has no trust boundary. CI runs gtest-parallel from a pinned upstream checkout; only `tests/compiler/run.sh` uses the vendored copy. See [[scorecard-reads-pruned-check-runs]].
