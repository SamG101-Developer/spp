---
name: test-runner-main-calls-std
description: "spp test's generated main (CompilerBoot, compiler_boot.cpp) calls std::os::env::get_env, OsStr::new and to_str_lossy; renaming or moving them breaks every spp test run"
updated: 2026-11-16
---

`spp test` compiles a `main` that the compiler writes as S++ text (the end of the test-runner builder in
`sources/spp/compiler/compiler_boot.cpp`). It reads `SPP_TEST_ONLY` with `std::os::env::get_env`, turns the result
into text with `OsStr::new` / `to_str_lossy`, and prints with `std::console::println`. These are the std names the
compiler depends on outside the prelude and intrinsics. Moving or renaming any of them breaks every `spp test`, in
every project, with an E26 in `tst/main.spp`. The gtest suite doesn't use the runner, so it stays green.

**Why:** on 2026-11-16, moving `get_env` from `std::process` to `std::os::env` (OsStr) made `spp test -l std` fail
at once in the generated file. Neither merge order is safe for such a move: the new compiler can't compile the old
std's runner, and the old compiler can't compile the new std's, unless std keeps the old name until the compiler is
out (see [[merge-spp-before-stl-for-new-features]]).

**How to apply:** when a std change touches one of these names, change the builder in the same piece of work. Keep a
forwarding definition under the old name in std, then merge std first and the compiler second; the compiler can't go
first, as its runner needs the new name. For the `get_env` move, the forwarder is `std::process::get_env` (lossy
text, marked transitional). Both runners passed std's suite on the shimmed std (2026-11-16). Remove it once the
compiler change is merged and released.
