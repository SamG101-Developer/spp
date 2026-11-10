---
name: kcfi-cold-functions-on-macho
description: "The KCFI hash word before a function belongs to the previous Mach-O atom; ld64 moving a cold function splits them. Fixed with prefix data, which makes the function an .alt_entry"
updated: 2026-11-10
---

LLVM writes a function's KCFI type hash as a bare word just before its label (`AsmPrinter::emitKCFITypeId`). On
Mach-O that word has no symbol of its own, so it belongs to the atom before it. A function LLVM marks `cold` gets
`N_COLD_FUNC` on its symbol (`MCSA_Cold`), and Apple's linker moves cold functions to the end of `__text`, without the
word in front of them. The check at an indirect call (`ldur w16, [x9, #-4]` against the expected hash) then reads
whatever is now there and traps with `brk #0x8229`, silently.

Seen 2026-11-10 on macOS STL conformance: `std::tst::future::test_an_async_call_runs_the_function` and
`std::tst::threading::thread::test_dropping_an_unjoined_handle_leaves_the_thread_its_result_cell`, each trapping in
`spawn::{closure#0}` when it called the user closure, which LLVM had made cold (the Linux object puts it in
`.text.unlikely.`). Cross-compiled objects for `aarch64-apple-darwin` and `aarch64-linux-gnu` both have the right hash
right before the closure (`0x3129e27c`), and the macOS symbol carries `ColdFunc (0x400)`. ELF keeps the word in the same
section as the function, so Linux is unaffected. Which functions turn cold depends on the code, so this comes and goes
as std changes.

**First attempt (2026-11-10), only half a fix:** removing `cold` from typed functions. The word belongs to the
*preceding* atom, so a cold function *before* a typed one still carried its hash away: CI then failed
`test_an_async_call_runs_the_function` and `test_joining_returns_an_owned_result_without_freeing_it`, whose closures
followed the cold `test_a_vector_formats_its_elements` and `test_dropping_an_rwlock_drops_its_value`.

**Fixed 2026-11-10:** LLVM already solves this for prefix data (`AsmPrinter::emitFunctionPrefix`: a linker-private
`ltmpN` label starts the atom and the function symbol becomes `.alt_entry`); `emitKCFITypeId` skips that path, which is
the upstream bug. `codegen::FinishCfi` (`llvm_harden_codegen.cpp`, after the optimiser under the `cfi` gate)
gives every defined function with `!kcfi_type` an `i32 0` of prefix data on Mach-O. The layout is `ltmpN`, `0`, hash,
function (alt_entry), all one atom, so `cold` stays and moving it is harmless; 4 bytes per typed function. Verified on
the cross object: 88 typed definitions, 88 `AltEntry` symbols, each with an atom label 8 bytes before it and the right
hash 4 bytes before it.

**Why:** the trap is in a correct object; only the macOS link breaks it, and it looks like a threading bug.

**How to apply:** if a typed function on Mach-O loses its `AltEntry` flag (a function given its own prefix data is
skipped, as LLVM labels it anyway), this comes back. Drop the workaround once `emitKCFITypeId` goes through
`emitFunctionPrefix` upstream. Diagnose a
macOS `brk #0x82xx` by cross-compiling (`spp test --target aarch64-apple-darwin --runner true` with an all-targets
build, `sls = false`) and checking the callee's symbol flags with `llvm-readobj --symbols`. See [[kcfi-bundles-not-metadata]].
