---
name: valgrind-runs-whole-stl
description: "Valgrind cell runs every library's !test suite (valgrind-conformance.sh), ~6 min locally; replaced the 12-group subset 2026-11-10"
updated: 2026-11-10
---

The Linux lanes' advisory "Valgrind Conformance" cell runs `.github/scripts/test/valgrind-conformance.sh`:
`spp test --all-libs --runner "valgrind --leak-check=full --errors-for-leak-kinds=definite --error-exitcode=9 ..."`
in a scratch project with `deny_wx` and `seal` off. It replaced `valgrind-groups.sh` (12 hand-picked `std` groups,
from M9.6, which assumed the whole suite was too slow). Timed locally on 2026-11-10 (WSL, valgrind 3.24, release spp):
5 min 43 s wall for 666 tests, including the build and `spp test`'s re-run of every test in its own valgrind process
after the first run failed. The cell's limit stays at 90 min.

A leak in any test fails the single run, so the re-run is what names the test; tests inlined into `main` by LTO show
`main::main` as the frame in the first run's records. First findings (SPP-STL master 4d89366): definite leaks in
`std::tst::variant::test_a_variant_can_hold_a_non_copyable_alternative` (a `Vec[S32]` buffer, the variant
`drop`ped explicitly) and three `std::tst::fs::path` tests (`parent`, `with_extension`, a clone; `Str` buffers from
`Vec::from_in`).

**Why:** the subset never ran those tests, so these leaks were invisible.

**How to apply:** read a valgrind failure from the re-run section (after "Re-running each test in its own process"),
not the first run's records. A suppression in `valgrind.supp` needs a reason that it is not generated code's fault.
