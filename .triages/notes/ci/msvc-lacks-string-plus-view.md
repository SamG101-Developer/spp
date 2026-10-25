---
name: msvc-lacks-string-plus-view
description: "Str + StrView compiles on libstdc++ (P2591) but not on the MSVC STL; MSVC-only C2678 at a concatenation"
---

`std::string + std::string_view` is C++26's P2591. libstdc++ implements it, so GCC/clang builds are green;
the MSVC STL does not, so the same line is a Windows-only `C2678: binary '+'` whose candidate list is a pile of
unrelated `numex::BigInt/BigDec operator+` overloads — the real message is "no `std::string + string_view`".

`const char* + StrView` is invalid everywhere, so only a chain whose *left* operand is already a `Str` hits this.
A macro that is not parenthesised makes that happen by accident: `"text " + INLINE_INFO(x)` re-associates into
`("text " + colex_pair) + x`, turning a standalone `Format + StrView` (which ColEx overloads, and which is fine)
into `Str + StrView`.

**How to apply:** convert a `StrView` before concatenating it, and parenthesise any macro that expands to a `+`
chain. Convert through a helper (`AsStr(StrView) -> Str`), not `Str(x)` at the splice: GCC's `-Wuseless-cast`
(`-Werror` in CI) rejects `Str(x)` when `x` is already a `Str` *prvalue* - an lvalue `Str` passes, so a local
one-line test will not reproduce it. See the `INLINE_INFO`/`INLINE_NOTE`/`INLINE_HELP` macros and `AsStr` in
`sources/spp/analyse/errors/semantic_error.cpp`. Related: [[msvc-strong-ordering-import-std]].
