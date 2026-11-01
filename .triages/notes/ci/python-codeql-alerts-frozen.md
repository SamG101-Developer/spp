---
name: python-codeql-alerts-frozen
description: "Python CodeQL alerts come from the removed default setup (last scan July 2026); _analysis.yaml scans c-cpp only, so edits cannot close them"
updated: 2026-11-01
---
The open `py/*` CodeQL alerts (`docs/doxygen_filter.py`, `tests/gtest-parallel/gtest_parallel.py`) were last updated on 2026-07-27 by GitHub's default setup (`dynamic/github-code-scanning/codeql:analyze`, category `/language:python`). Default setup is now `not-configured`, and `_analysis.yaml` runs CodeQL with `languages: c-cpp` only, so no Python file has been scanned by CodeQL since.

**Why:** fixing the flagged line does not close these alerts, because no scan runs to confirm the fix. The repo's Python scripts also have no CodeQL coverage.

**How to apply:** close them by dismissing them or by bringing Python scanning back; a code edit alone will not. `.github/codeql-config.yaml` ignores `tests`, so a new Python scan would not cover `tests/gtest-parallel`. CI runs gtest-parallel from a pinned upstream checkout; only `tests/compiler/run.sh` uses the vendored copy. See [[scorecard-reads-pruned-check-runs]].
