---
name: clang-cl-std-apply-mangling-crash
description: "clang-cl 23.1.5 segfaults mangling std::apply mid-template-instantiation; types.ixx no longer re-exports apply; reproducers upload with the build logs"
updated: 2026-11-08
---

clang-cl 23.1.5 (MSVC STL 14.51, `import std`) crashes with 0xC0000005. The stack ends in "LLVM IR generation of
declaration 'std::apply'" / "Mangling declaration 'std::apply'" (`tuple:1248`), while it is doing something unrelated:
instantiating `Variant<numex::BigInt, …>`'s destructor in `comp_key.ixx:56`, or a `genex::views::transform` pipeline
in `lsp/diagnostic.cpp:320`. clang on Linux and macOS (Itanium mangling) does not crash. Removing
`SPP_EXP_FUN using std::apply;` from `types.ixx` (2026-11-08) got `comp_key.ixx` through, but genex's `bind_back` calls
`std::apply` itself, so other files can still hit it. No upstream report matched.

**Why:** the clang-cl lane is advisory; fixing it means a reduced reproducer for LLVM, not more source workarounds.

**How to apply:** don't re-export `std::apply` (the banned-idioms rule no longer asks for `apply`). On a crash, the
failed-build artifact `buildlog-<os>-<compiler>-<variant>` has `crash/` with clang's `.cppm`/`.cpp` and `.sh`
reproducers (`collect-crash-reproducers.sh`). The `.sh` still names the module BMIs, which are not included.
