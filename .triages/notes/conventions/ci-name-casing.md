---
name: ci-name-casing
description: "CI names: Title Case for workflows that trigger runs and every job/matrix label; sentence case for steps; kebab-case for identifiers"
updated: 2026-10-25
---

Display names in `.github`:
- **Workflows that trigger runs** (`PR`, `Nightly`) and **every job name**, in callers, inside reusable workflows, and
  in matrix labels (the `platforms` matrix, `plan-tests.sh` labels): Title Case. `(Advisory)` and other bracketed
  qualifiers are Title Case too.
- **Step names:** sentence case (`Upload test logs on failure`).
- **Identifiers** (job ids, artifact names, composite action names, a reusable `_*.yaml` workflow's own `name:`):
  lowercase kebab-case.
- Tool and platform names keep their own spelling: GCC, Clang, MSVC, clang-cl, macOS, CodeQL, Cppcheck, pre-commit.

Branch protection requires `CI Success` and `PR Title Format` by exact name: renaming either needs the ruleset
changed too.

**Why:** the maintainer chose this (2026-10-25) after the checks list mixed `Linux GCC`, `linux-arm gcc` and inner
`build`; a PR shows `PR / Linux GCC / Build` with one style throughout.

**How to apply:** name new jobs and matrix entries in Title Case. `plan-tests.sh` runs on macOS before the newer bash
is installed, so stick to bash 3.2 there (no `${var^}`).
