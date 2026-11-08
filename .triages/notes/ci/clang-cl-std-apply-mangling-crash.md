---
name: clang-cl-std-apply-mangling-crash
description: "clang-cl 23.1.5 segfaults mangling std::apply mid-template-instantiation (comp_key.ixx, lsp/diagnostic.cpp); not caused by the apply re-export; reproducers + BMIs in the buildlog artifact"
updated: 2026-11-08
---

clang-cl 23.1.5 (MSVC STL 14.51, `import std`) crashes with 0xC0000005. The stack ends in "LLVM IR generation of
declaration 'std::apply'" / "Mangling declaration 'std::apply'" (`tuple:1248`), while it is doing something unrelated:
instantiating `Variant<numex::BigInt, …>`'s destructor in `comp_key.ixx:56`, or a `genex::views::transform` pipeline
in `lsp/diagnostic.cpp:320`. clang on Linux and macOS (Itanium mangling) does not crash. Removing `SPP_EXP_FUN using std::apply;` from `types.ixx` was tried (2026-11-08, then restored) and did not help: `comp_key.ixx` still crashes (a run that stopped at `diagnostic.cpp` first had not compiled it). genex's `bind_back` calls `std::apply` too. No upstream report matched.

**Why:** the clang-cl lane is advisory; fixing it means a reduced reproducer for LLVM, not more source workarounds.

**How to apply:** don't remove the `apply` re-export again for this. On a crash, the
failed-build artifact `buildlog-<os>-<compiler>-<variant>` has `crash/` with clang's `.cppm`/`.cpp` and `.sh`
reproducers (`collect-crash-reproducers.sh`). The `.sh` names ~160 module BMIs (std, genex, numex, spp); the collector copies them to the same paths (relative to the build tree) under `crash/`, so the `.sh` replays from that folder, given the same MSVC STL and LLVM headers the BMIs were built against. Nothing calls `std::apply`: frames 14-15 are CodeGen being handed its declaration mid-instantiation. The path in clang 23.1.5's source (read 2026-11-08): while deserializing, `ASTReader::isConsumerInterestedIn` passes every `FunctionDecl` with a body to CodeGen's `HandleTopLevelDecl`; `EmitTopLevelDecl` skips it only if `isTemplated()`; otherwise `EmitGlobal` mangles it. No module comp_key imports instantiates `std::apply` (NumEx has no genex; `spp.utils.types`/`stats` don't call it), so the likeliest reading is the `std::apply` *pattern* reaching CodeGen before its template link is wired up mid-deserialization, and the Microsoft mangler failing on its dependent signature where Itanium copes (macOS runs the same 23.1.5 without crashing). Unconfirmed: the release clang-cl has no assertions or symbols, so frame 0 is unnamed. The trigger in comp_key is instantiating `std::variant<numex::BigInt, numex::BigDec, bool>`'s destructor (`_Variant_raw_visit_noexcept` → `is_nothrow_invocable`). When reading a run, check the file was compiled at all: ninja stops at the first failure.
