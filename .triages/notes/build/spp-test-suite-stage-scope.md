---
name: spp-test-suite-stage-scope
description: "SPP gtest suite now runs every stage through codegen and verifies the IR (since 2026-09-12); nothing is optimised, linked or executed"
updated: 2026-09-13
---

**Changed 2026-09-12 (the rest of this note is history):** `ForCppGoogleTest` sets `CompilerBoot::VerifyOnly`, and
`Compile()` no longer skips 9.5/10/11 for unit tests. Stage 11 verifies every module and the LTO-linked module, then
stops: no `.ll` files written (parallel tests share `tests/test_outputs`), no optimisation, emit or link. Invalid IR or
a failed LTO link raises an internal compiler error (`spp::Ice`, since 2026-10-17; was `std::runtime_error`) (not `analyse::errors::error`, so a SHOULD_FAIL test cannot pass on it);
a codegen segfault shows up as a crashed test. Cost: ~1.23x per test on a 60-test sample. First full run: 61 failures =
the 24 already red on HEAD + 37 new (codegen crashes, verifier failures, ICEs, and analysis errors from Stage 9.5
analysing generic instantiations for the first time). Running the program is deliberately not done: every SHOULD_PASS
test gets an empty `main`, so it would prove nothing; runtime values belong in the s++ `!test` suites.

**Fixed 2026-10-16 (N1.12): `CompilerBoot` owns the `LLVMContext` now, one per compile; the rest of this paragraph is history. Memory no longer grows per compile either (N1.13, same day: `~Compiler` clears the interned identities and memos), so long `--gtest_filter` batches in one process work.** Several tests in one process leaked LLVM state (2026-09-13): `GLOBAL_CONTEXT` (`llvm_ctx.ixx`) is one `LLVMContext`
per process, and `llvm_type.cpp` reuses a named struct that already exists. So a `--gtest_filter` running many tests in
one `spp_tests` process can hand a test a previous test's same-named type: `test_valid_return_type_inheriting_generator`
got the prior test's empty `main::MyType` and failed the verifier. gtest-parallel runs one test per process, so the
full suite never sees it. Rerun a test solo before treating an in-filter codegen failure as a regression. **Give each
new test its own class names for the same reason** (2026-09-25): two tests I added to one file both declared `cls A`,
one empty and one with a field, and the second failed `Invalid indices for GEP pointer type` on a perfectly valid
program - it had been handed the first test's empty `%"main::A"`.

`SPP_TEST_SHOULD_PASS_SEMANTIC`/`FAIL_SEMANTIC` (via `build_temp_project` -> `spp::cli::unit_test` ->
`Compiler::ForUnitTests` -> `Compile()`) runs Stage1 through Stage9_CompTimeResolve **only**: `Compile()` guards
Stage9_5_Monomorphise / Stage10_PreCodeGen / Stage11_CodeGen behind `if (not m_for_unit_tests)`, which
`ForUnitTests` sets. So no codegen, no `llvm::verifyModule`, no linking, no execution.

**Why:** verified 2026-08-20 in `sources/spp/compiler/compiler.cpp`. (Corrects the earlier note here claiming the
suite ran through Stage11 and caught malformed IR - it does not.) The user plans a Stage10/11 execution suite,
including a way to read a resolved `cmp` value back out so comptime results can be asserted (e.g. 50th Fibonacci
number, other math problems), for every ast that can be `cmp`'d through.

**How to apply:** for a codegen (Stage10/11) fix, running the compiler over `./project` (see [[build-workflow]]) is
the only real check - a green gtest run says nothing about it. Say so plainly rather than implying the suite covers
it. Note the split means a crash in a unit test is in stages 1-9, even when `./project` compiles the same source
fine, because `./project` reaches those stages with a different set of instantiations already created.
