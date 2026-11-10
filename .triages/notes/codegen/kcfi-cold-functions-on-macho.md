---
name: kcfi-cold-functions-on-macho
description: "ld64 moves cold (N_COLD_FUNC) functions away from the KCFI hash word before them; KeepCfiTypesAttached keeps typed functions off cold on Mach-O"
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

**Fixed 2026-11-10:** `codegen::KeepCfiTypesAttached` (`llvm_harden_codegen.cpp`), run after `CheckCfiCalls` under the
same `cfi` gate, removes `cold` from every function with `!kcfi_type` when the target is Mach-O. Untyped cold functions
(direct calls only, such as `Vec::grow_one`) keep it. Verified on the cross-compiled object: the closure's flags are
`0x0` and its hash is still in front of it. The upstream layout (a bare word before the label) is unchanged, so clang's
own `-fsanitize=kcfi` on Darwin has the same exposure.

**Why:** the trap is in a correct object; only the macOS link breaks it, and it looks like a threading bug.

**How to apply:** anything that moves a typed function away from the word in front of it (an order file, `-dead_strip`, a
section attribute) brings this back on Mach-O. Diagnose a
macOS `brk #0x82xx` by cross-compiling (`spp test --target aarch64-apple-darwin --runner true` with an all-targets
build, `sls = false`) and checking the callee's symbol flags with `llvm-readobj --symbols`. See [[kcfi-bundles-not-metadata]].
