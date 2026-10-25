---
name: hardening-needs-feature-gate
description: Any hardening measure that can break a niche setup ships default-on behind an spp.toml key, failing loudly rather than degrading
updated: 2026-10-21
---

Every memory-safety/hardening measure that can be a problem in some niche scenario (old kernel, child that JITs,
unusual platform) goes through the feature system: a `ConfigKey` + schema `KeySpec`, default on. Don't add runtime
fallbacks that quietly skip the measure; if the environment can't support it, fail with a message naming the key to
turn it off.

**Why:** the maintainer, on S16.6 MDWE (2026-10-17): always apply `PR_MDWE_NO_INHERIT`; kernels < 6.6 "will have to use the
feature flag to disable it ... as we have seen with other memory features". Matches S0.4 (never a silent no-op).

**Platform floor (2026-10-21):** the maintainer wants old systems supported, floor RHEL 8 (Linux 4.18, glibc 2.28, binutils
2.30). Resolved as a *build-time* floor, not a runtime fallback: `[target.floor]` in spp.toml leaves out a measure the
floor can't have (deny_wx 6.6, seal 6.10) and records it in the hardening record; an explicit key above the floor is a
build error. The default floor is the release floor, so by default deny_wx/seal are left out. Runtime still never skips a measure. A fallback that does the same *operation* another way (io_uring ->
plain syscall) is fine. Plan: `../../docs_wip/ci-production-readiness-plan.md` P1. Implemented 2026-10-21: a run-time measure's `KeySpec.MinLinux` + `features::Applied`/`LeftOut`/`CheckFloor`; a new
one only needs `.MinLinux` and its `Applied` call where the shim is built.

**How to apply:** a compile-time measure is gated with `FEATURE_GATE` in `compiler_boot.cpp`; a runtime one becomes an
`int(void)` sppc step appended in `RuntimeInitShim` (`llvm_entry.cpp`), passed down through `EmitCEntryPoint` like
`split_stacks`/`deny_wx`. Related: [[spp-toml-feature-schema]], [[one-hardening-measure-at-a-time]].

Valgrind runs in-process, so `deny_wx` (MDWE refuses its JIT's exec mmaps: "out of memory: Permission denied") and `seal` (valgrind 3.24 lacks syscall 462) must be off under it; `.github/scripts/test/valgrind-groups.sh` writes both into its scratch spp.toml. `s++ test` only honours spp.toml since 2026-10-18 (HandleTest skipped `features::Load`).
