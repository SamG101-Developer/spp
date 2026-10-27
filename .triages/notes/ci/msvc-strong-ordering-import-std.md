---
name: msvc-strong-ordering-import-std
description: "MSVC LNK2019 on std::strong_ordering::less/equal/greater means the @cmake_cxx_std import-std library was dropped from the link, not that the source names them"
---

`LNK2019: unresolved external symbol ?less@strong_ordering@std@@2U12@B` (and
`equal`, `greater`) on the MSVC runner is **not** a source problem. Those
constants are attached to module `std`, so their only definition lives in the
object CMake builds for `import std` — the library it names `@cmake_cxx_std`.
On Windows that is the entire file name, and `cmake -E vs_link_exe`, which
every MSVC link is routed through, treats any argument beginning with `@` as an
nmake response file (`cmcmd.cxx`, `cmcmd::VisualStudioLink`; still there in
4.4.0). It opens the `.lib` as text, expands it to nothing, and link.exe never
sees it. Compare the two command lines in the log: the one CMake composed has
`@cmake_cxx_std.lib`, the one that ran does not.

Anything defined only in the std module goes missing this way; the
`strong_ordering` constants are just the ones almost every program reaches,
because MSVC's builtin integral `<=>` **also** emits references to them.

**Why:** the earlier reading of this — that naming the constants was the fault,
and that builtin `<=>` was safe — was wrong. The `ordering(sign) { return sign
<=> 0; }` helper in NumEx `sources/numex/big_int.cpp` was written on that
belief and changed nothing, because the builtin `<=>` needs the same three
symbols. It is harmless, but it is not the fix.

**How to apply:** keep the archives out of the build root so the `@` is never
the first character of an argument — `set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
"${CMAKE_BINARY_DIR}/lib")` under `if (MSVC)` in the root CMakeLists, and
`-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=lib` in `install-small-libs.sh` for the
dependency builds. A GMF `#include <compare>` would also work by moving the
constants to the global module, but that is against
[[import-std-not-stl-headers]] and only patches one symbol.
