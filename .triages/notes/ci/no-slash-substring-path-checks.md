---
name: no-slash-substring-path-checks
description: "Path string checks like contains(\"/src/std/\") never match on Windows; compare path components or set a flag in ModuleTree"
updated: 2026-10-25
---

On Windows, every path the compiler gets from `directory_iterator`/`GlobSpp` uses `\`, so a string test such as
`NativeString(p).contains("/src/std/")` is silently always false. That check in `CompilerBoot::Lex` appended the
prelude to every std source on MSVC, and each std file's own `use std::annotations::public` then raised E14 "The
prelude already imports ..." in almost every test. It has been replaced by `Module::IsStd`, which `ModuleTree` sets from
the path components it already splits (`lib_name == "std"` and top folder `src`).

The same trap was fixed in three more places: `CheckErrorLocations` (`tests/compiler/test_macros.hpp`) checked
`ends_with("/src/main.spp")`, so every should-fail test failed on Windows; the `vcs` check in
`test_msc_json_diagnostics.cpp` passed on Windows without testing anything; and `CompilerFileOf` (`utils/ice.cpp`)
showed MSVC's full `source_location` path. A test that reads a path out of error text should parse it as `spp::Path`.

**Why:** Linux CI can't catch it, and the failure shows up far from the check (as a duplicate-identifier error in a std file).

**How to apply:** decide where a module comes from in `ModuleTree` (from `Owner`, `IsFfi`, `IsStd`, or by comparing
`Path` components as its vcs filter does), never by searching a path string for `/`. If a string test is truly
needed, check both separators, as `lsp/resolution_index.cpp` does.
