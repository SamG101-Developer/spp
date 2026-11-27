---
name: spp-sup-generics-from-receiver
description: "Blanket \"sup [T] T\" and pack sup generics stay unbound at attach; a call binds them off its receiver (ReceiverGns, UnifyInHierarchy), then the block's own (BoundGnsOf)"
updated: 2026-11-19
---

Attaching a `sup` block to a type (`sup_attachment::AttachSpecificSuperScopesImpl`) binds the block's generics by
matching the type against its pattern. Two kinds stay unbound:
- **A trailing variadic pack** (`sup [First, ..Rest] Tup[First, Rest]`). The block is attached with its pack unbound:
  it stands for every element set at once.
- **A blanket block's own type** (`sup [T] T`). Blanket blocks take the unsubstituted branch, on
  `not genex::contains(GnSupBlocks, sup_scope)`.

A method in such a block reached codegen still generic (`f[..Rest](&self)`) until 2026-09-12: "Call target has no llvm
declaration" (13 gtests once codegen ran).

**How a call binds them now** (`overload_resolution::CheckCandidate`, as given layers below the written arguments; see
[[generic-inference-engine-shape]]):
- **`ReceiverGns`** matches the receiver against the block's pattern: the receiver itself, else the first class it is
  superimposed as (`type_members::UnifyInHierarchy`). It keeps only the block's own parameters (`BindingsFor`). A
  blanket `T` binds to the receiver; a pack binds to the receiver's remaining arguments as one `Tup[...]`, the form a
  variadic generic is bound to everywhere else. It replaced `BindBlanketAndPackGenericsFromReceiver` (2026-09-12),
  which bound them separately.
- **`BoundGnsOf(ReachedThrough)`** then gives what the block binds already. It comes after, because the attached scope
  offers unbound identity bindings that would otherwise win (first binding wins).

**Traps:**
- A candidate's `FnScope` is the method's own lowered `sup $F ext FunXxx { fun }` block, not the block it was written
  in. `sup_blocks::EnclosingSupBlock` walks up past `$`-patterned blocks to it.
- `asts::AstName` *throws* ("ast_name: Unsupported AST type ModulePrototypeAst") on anything but a type or sup node, so
  reach it through `EnclosingSupBlock`, never on an arbitrary scope's node.

**Binding these at attach time instead was designed and dropped (2026-09-14).** It needed a thread-local recursion
guard (`sup [T] T ext CanBorrowRef[T]` recurses forever), pack rechecks against the template scope (a bound `Items` no
longer counts as a pack), and the receiver binding anyway, for the level the guard cuts off. That's more machinery
than it deletes.

Related: [[spp-dollar-mock-types]], [[tuple-sup-matching-broken]], [[generic-param-sym-links-dummy-scope]].
