---
name: gcc-modules-vtable-key-function
description: GCC can leave a module class's vtable undefined ("undefined symbol: vtable for X") after an unrelated edit; SPP_GCC_VTABLE_FIX gives it a key function in the interface
updated: 2026-11-07
---

With GCC and C++20 modules, a class exported from a module interface can link as `undefined symbol: vtable for
spp::asts::X` after an edit that does not touch its virtual functions. On 2026-11-04 it happened to
`PostfixExpressionAst` when a second exported struct (`PackElementCloneGuard`) was added after it in
`postfix_expression_ast.ixx`. The object of the class's own `.cpp` then held the vtable only as `U` (`nm -C`), so
nothing emitted it. On 2026-11-07 the switch of `use()` from `using namespace` to a using-declaration did it to
`ExpressionAst`, `ClassPrototypeAst`, `LetStatementInitializedAst` and `PostfixExpressionOperatorFunctionCallAst` at
once; touching and rebuilding did not help, the fix below did. Qualifying the `.cpp` definitions for MSVC
(`auto spp::asts::X::F`, the same day) then did it to `LoopExpressionAst`, `DeferStatementAst` and `CmpStatementAst`;
putting one destructor's definition back to the bare name did not bring its vtable back.

**Why:** GCC emits a polymorphic class's vtable where its key function is defined, and with modules that decision can
fall between the interface and its implementation unit.

**How to apply:**
- Add `SPP_GCC_VTABLE_FIX` as the first line of the class body, and `SPP_GCC_VTABLE_FIX_IMPL(spp::asts::X)` at the
  end of the `.ixx` (`headers/spp/macros.hpp`). This defines an empty key function in the interface, so the vtable is
  emitted there. `TupleLiteralAst` and `SpreadExpressionAst` already have it.
- Check with `nm -C <obj> | grep "vtable for"`: `U` means referenced but not defined.
- It is GCC-only; the macros are empty on other compilers.
