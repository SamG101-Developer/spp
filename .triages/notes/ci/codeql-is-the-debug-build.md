---
name: codeql-is-the-debug-build
description: "CodeQL (_analysis.yaml) is CI's only -g build (Debug, Linux GCC); the build matrix is all Release, so debug-info ICEs surface only there"
updated: 2026-10-27
---

`_analysis.yaml` builds CodeQL with `build-type: Debug` (Linux, Homebrew GCC), so it's the only CI build that compiles with `-g`. Every lane (`_lane.yaml`, `_coverage.yaml`, `_cross_codegen.yaml`) uses the default `Release`. It runs from `pr.yaml`, `post_merge.yaml` and `nightly.yaml`.

**Why:** GCC module bugs in debug-info generation (for example, [[gcc-opaque-enum-dwarf-ice]]) break only `-g` builds, so a CodeQL-only build failure with green build lanes is usually one of these, not a CodeQL problem.

**How to apply:** for a CodeQL build failure, reproduce it with the local Debug GCC build (`cmake-build-debug-*`). Keep CodeQL on Debug: it's the Debug coverage, and no separate Debug build lane is needed.
