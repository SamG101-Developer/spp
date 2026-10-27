---
name: spp-test-harness-stale-std
description: "tests/test_outputs/vcs is pulled once per run by SppBootstrap.Fixture (since 2026-10-18); a run without the fixture compiles whatever is checked out"
updated: 2026-10-18
---

**Since 2026-10-18 (codegen-test-plan P2): `SppBootstrap.Fixture` pulls.** It calls `EnsureTempProject(true)`
(`tests/compiler/test_boot.cpp`), which runs `HandleVcs` on an existing `vcs/` - and `HandleVcs` pulls an existing
checkout. `run-unit-tests.sh` and CI's `gtest.sh` run the fixture first, so a run is refreshed once at its start. Every
other test process calls `EnsureTempProject()`, which clones only an empty `vcs/` - one pull per test process would be
~2000 pulls. **So a hand-run `spp_tests --gtest_filter=X` without the fixture compiles whatever is checked out**; run
`--gtest_filter=SppBootstrap.Fixture` first, or include it in the filter.

A failed pull fails the fixture test; `run-unit-tests.sh` warns and continues on the existing checkout (stops only
when `vcs/` is empty). A hand-edited `tests/test_outputs/vcs/std` can make the pull fail - The maintainer accepted that.

**Stubs are read from `vcs/<dep>/ffi` directly** (`FfiFolders`, `sources/spp/compiler/ffi.cpp`); the fixture's own
`ffi/` is empty, so a pull is the whole refresh. A full re-clone is still `rm -rf tests/test_outputs/vcs`. Never
hand-copy sppc files: since the per-target layout (`ffi/sppc/x86_64-linux-gnu/libsppc.so`, plus `SOURCE`) a flat copy
fails validation.

Why it mattered (2026-09-04): a frozen copy still had `0xFFFFFFFF_u32`, which `1da2ff0c "Tighten hex char parsing"`
made a syntax error, so all 29 `TestMscVisibility` tests failed on a parse error in `std/bignum/biguint.spp`.

Also worth knowing: `spp_tests` runs many `build_temp_project` compiles **in one process**, so any process-wide
compiler state has to be reset per compile. A compile that throws (every `SPP_TEST_SHOULD_FAIL_SEMANTIC`) never
reaches the later stages, so anything queued earlier leaks into the next test and gets reported against a different
program. This is why `visibility_utils::ResetDeferredTypeVisibilityChecks` is called from
`CompilerBoot::Stage2_GenTopLvlScopes`. Running `./project` cannot catch that class of bug - it compiles once.

**A local std edit is invisible to the gtest suite.** The clone's origin is the GitHub SPP-STL, not
`project/vcs/std`, so work in progress in the local std reaches `./project` and nothing else - and now the fixture's
pull would conflict with a hand copy, so push the std change instead. Also rebuild the `spp_tests` target, not just
`spp`, after a cli.cpp change.

Related: [[spp-test-suite-stage-scope]], [[spp-test-writing-idioms]], [[build-workflow]].
