---
name: clang-cl-std-apply-mangling-crash
description: "clang-cl 23.1.5 segfaults mangling std::apply mid-template-instantiation (comp_key.ixx, lsp/diagnostic.cpp); not caused by the apply re-export; reproducers need the BMIs"
updated: 2026-11-08
---

clang-cl 23.1.5 (MSVC STL 14.51, `import std`) crashes with 0xC0000005. The stack ends in "LLVM IR generation of
declaration 'std::apply'" / "Mangling declaration 'std::apply'" (`tuple:1248`), while it is doing something unrelated:
instantiating `Variant<numex::BigInt, …>`'s destructor in `comp_key.ixx:56`, or a `genex::views::transform` pipeline
in `lsp/diagnostic.cpp:320`. clang on Linux and macOS (Itanium mangling) does not crash. Removing `SPP_EXP_FUN using std::apply;` from `types.ixx` was tried (2026-11-08, then restored) and did not help: `comp_key.ixx` still crashes (a run that stopped at `diagnostic.cpp` first had not compiled it). genex's `bind_back` calls `std::apply` too. No upstream report matched.

**Why:** the clang-cl lane is advisory; fixing it means a reduced reproducer for LLVM, not more source workarounds.

**How to apply:** don't remove the `apply` re-export again for this. On a crash, the
failed-build artifact `buildlog-<os>-<compiler>-<variant>` has `crash/` with clang's `.cppm`/`.cpp` and `.sh`
reproducers (`collect-crash-reproducers.sh`). The `.sh` names ~160 module BMIs (std, genex, numex, spp) that are not included, so it does not replay on its own. When reading a run, check the file was compiled at all: ninja stops at the first failure.
