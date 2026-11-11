---
name: msvc-needs-member-type-imported
description: "Calling through a member whose type another module defines (postfix->Op->To<..>()) needs that module imported; GCC/clang reach it transitively, MSVC errors C2027"
updated: 2026-11-10
---

`postfix->Op->To<X>()` with only `spp.asts.postfix_expression_ast` and X's module imported builds on GCC and clang,
but MSVC (x64 and ARM) fails with C2027 "use of undefined type 'spp::asts::PostfixExpressionOperatorAst'" and C2039
"'To': is not a member of 'std::unique_ptr<...>'": `Op`'s pointee is defined in
`spp.asts.postfix_expression_operator_ast`, which GCC and clang make reachable through the imports but MSVC does not.
Seen in run 42738268101 (2026-11-10, `linear_utils.cpp`'s `IsLinearTemporary`).

**Why:** the Linux and macOS builds pass, so the failure first shows on the Windows lanes, after a push.

**How to apply:** when a file calls a method on a member (`->Op`, `->Lhs`) of an AST it imports, import the
module defining that member's type too, as `regions.cpp` does for every postfix operator it touches.
