---
name: clang-tidy-ci-job
description: "clang-tidy runs advisory in _analysis.yaml on the Linux Clang lane's restored tree: PRs check changed files, nightly every file; one script shared with .tools"
updated: 2026-11-12
---

The `Clang-Tidy (Advisory)` job in `_analysis.yaml` restores the Linux Clang lane's release build tree (ubuntu-24.04,
clang at the `llvm-lib` pin, variant `release`), configures with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, builds, and runs
`.github/scripts/scan/clang-tidy.sh build`. On a pull request it passes `--since <base sha>`, which checks only the
changed `.ixx`/`.cpp` files. It falls back to every file when a `.hpp` or `.clang-tidy` changed, because those reach
every TU. Nightly passes `full-scan`, which checks every file. `pr.yaml` runs it behind the `clang_tidy` change gate.
`.tools/run-clang-tidy.sh` is now a wrapper that runs the same script over `cmake-build-release-wsl-clang`.

**Why:** clang-tidy has to load the clang build's module files, so unlike cppcheck it cannot run as a cheap gate before
the builds. It reuses a build tree the way the Sonar job reuses the GCC one. It is advisory (`continue-on-error`)
because the codebase still has findings: 42 in the 2026-11-08 local report, mostly `bugprone-macro-parentheses`, plus
whatever the newly enabled `readability-operators-representation` finds.

**How to apply:** make the job blocking (drop `continue-on-error` and "(Advisory)") once a full run is clean. A local
run whose report is mostly `clang-diagnostic-error` means the clang tree is stale (missing `.modmap` files, sources
renamed since it was configured). Rebuild that tree first, not the report.
