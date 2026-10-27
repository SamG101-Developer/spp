---
name: cpp-linters-local-only
description: "REVERSED 2026-08-18: cppcheck is now a blocking CI gate in pr.yaml. The earlier local-only rule no longer holds"
updated: 2026-08-18
---

**This reverses the earlier local-only rule.** On 2026-08-18 the user explicitly asked for cppcheck as a CI job in
`pr.yaml`, gating the builds ("no point wasting build time on bad code"), ordered after the cheap PR checks. So
`_cppcheck.yaml`, `.github/scripts/{setup/install-cppcheck,scan/cppcheck}.sh`, the pinned build-from-source + cache pipeline and
the `pr.yaml` job all exist again, and `cppcheck` is in the `ci-success` needs list.

Note this is the second time round: the same pipeline was built and then deleted on 2026-07-25 for cost reasons. It
was reinstated deliberately, not by accident — do not "restore" the local-only arrangement without being asked.

**Why:** the user's stated priority changed to failing fast before spending build minutes across five toolchains.

**How to apply:** treat CI-side C++ linting as wanted. Two findings from building it that still hold:
- Distro cppcheck is far too old for this codebase (Ubuntu 24.04 ships 2.13, which rejects `--std=c++26`), so it is
  built from source at a pinned commit from `cppcheck-opensource/cppcheck` (the repo moved from `danmar/cppcheck`).
- `--enable=style` is unusable here: cppcheck has no whole-program view across C++20 modules, so exported members come
  back as `unusedStructMember` (91 false positives). The gate runs `warning,performance,portability` only, and
  `syntaxError`/`unknownMacro` are suppressed as module-parse artefacts.

See [[build-workflow]].
