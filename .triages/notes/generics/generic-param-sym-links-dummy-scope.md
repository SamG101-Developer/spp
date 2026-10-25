---
name: generic-param-sym-links-dummy-scope
description: "A generic parameter's TypeSymbol has a non-null LinkedScope (its dummy scope) - test Kind==GenericParam for \\\"declared, unbound\\\"; and methods' inherited sup params"
updated: 2026-09-14
---

`GenericParameterTypeAst::Stage2` builds the parameter's `TypeSymbol` with its dummy scope as the third ctor arg,
which is `LinkedScope`. So `IsTypeGeneric() and LinkedScope == nullptr` is never true for a parameter; a check
written that way silently never fires. A whole bulk test run (build 70, 2026-09-14) tested nothing because of it.

**How to apply:** "declared and unbound" is `sym->Kind == TypeKind::GenericParam`. A binding made by
`CreateGenericSym` is `TypeKind::GenericArg`, and its `LinkedScope` is the bound value's (the dummy scope again when
bound to another generic, as in `T=T`). For comp generics: `GnCompParam`, or `BoundCompValue() == nullptr`.

Related design (2026-09-14): a method inside a generic `sup` still gets the block's parameters copied into its own
`GnParamGroup` (Stage1_PreProcess). That copy is what gives each block instance its own method instance, so it can't
be removed. The copies are marked `GenericParameterAst::IsInherited`, carried by every param `Clone`. Inherited params
register no symbol, so the body resolves `T` to the block's own. The bandaids that look related have other root
causes, so this change doesn't remove them:
- `BindBlanketAndPackGenericsFromReceiver`: attachment never binds blanket or pack generics.
- The `DetermineOverload` merge: this is how the inherited params get bound.

**Superseded:** the "generic identity = dummy scope" layer (builds 72-84: `GenericBindingScope`,
`GenericDeclaringScopes`, open instances registered in their declaring scope) was removed by the resolved-types
rewrite, and the `TypeEq` spelling match it said must stay is gone. Identity is now `ParamId`/`BindsParamId` plus
stamps; see [[resolved-types-generic-identity]]. `ScopeParentSwap` is also gone: a `sup` instance clones its member
subtree, so the prune uses the instance's own `$F` block (`OwningBlockOf`).

See [[generic-leak-shared-instantiations]], [[spp-sup-generics-from-receiver]].
