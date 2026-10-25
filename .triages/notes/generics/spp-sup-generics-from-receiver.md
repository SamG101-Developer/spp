---
name: spp-sup-generics-from-receiver
description: "Blanket \\\"sup [T] T\\\" and variadic-pack sup generics were never bound on a method call; fixed by reading them off the receiver in overload resolution"
updated: 2026-09-14
---

Attaching a "sup" block to a type (`AttachSpecificSuperScopesImpl`, scope_manager.cpp) binds its generics by
`RelaxedTypeEq` - except two kinds, which stay unbound: a trailing variadic pack (`sup [First, ..Rest] Tup[First, Rest]`
- the arity match checks the pack's constraints but never binds it) and a blanket block's own type (`sup [T] T` - blanket
blocks are excluded from substitution, line ~246). A method in such a block then reached codegen still generic
(`f[..Rest](&self)`), so no declaration existed: "Call target has no llvm declaration" (13 gtests once codegen ran).

**Fix (2026-09-12):** `BindBlanketAndPackGenericsFromReceiver` in overload_resolution.cpp, called per candidate in `DetermineOverload`
*before* `candidate.SupGns` is merged (MergeGenerics is first-wins, and the attached scope offers unbound identity
bindings that would otherwise win). Blanket: bind `T` = receiver. Pack: bind it to `TupleType(...)` of the receiver's
remaining args - the same `Tup[...]` form a variadic generic is bound to everywhere else.

**Traps hit on the way:**
- A candidate's `FnScope` is the method's own lowered `sup $F ext FunXxx { fun }` block, not the block it was written in;
  walk up past `$`-patterned blocks to find the generics.
- `asts::AstName` *throws* ("ast_name: Unsupported AST type ModulePrototypeAst") on anything but a type/sup node - guard
  the walk to `SupPrototypeFunctionsAst`/`SupPrototypeExtensionAst` before calling it.

**Binding these at attach time instead was designed and dropped (2026-09-14).** Blanket blocks are sent to the
unsubstituted branch by `not genex::contains(GnSupBlocks, sup_scope)` in `AttachSpecificSuperScopesImpl` (no
recorded reason). Packs are only arity-checked by `MatchArgListArity`. Doing it at attach time needs three things:
- a thread-local recursion guard, since `sup [T] T ext CanBorrowRef[T]` recurses forever
- pack rechecks run against the template scope, because a bound `Items` no longer counts as a pack
- this function anyway, for the level the guard cuts off

That's more machinery than it deletes, so keep this function.

Related: [[spp-dollar-mock-types]], [[tuple-sup-matching-broken]], [[generic-param-sym-links-dummy-scope]].
