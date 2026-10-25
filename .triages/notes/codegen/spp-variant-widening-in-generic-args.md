---
name: spp-variant-widening-in-generic-args
description: "Analysis accepts an alternative in a generic argument, so codegen has to widen it - and where it still does not"
---

**The rule the two halves have to agree on.** An alternative is matched against a variant's member with the variant
rule turned off, but *only at the top level*: `TypeEq`'s recursion into the generic arguments below it turns
`check_variant` back on (see `check-variant-is-single-level`). So `Pass[Opt[T]]` answers to a `Pass[Some[T]]` and the
call type-checks. That is wanted - it is how a value written as one alternative is accepted at all - but it means code
generation cannot assume a value has the alternative's layout just because the alternative matched.

**Fixed 2026-09-06.** Three things, all in what is now `spp.codegen.llvm_variant` (split out of `llvm_type`, which is
about lowering types rather than about converting values into one):

- `CoerceToVariant` compares the *lowered* types before tagging. When they differ, the value is rebuilt as the
  alternative first (`CoerceStructurally`) rather than copied into the payload as it stood. Copying it as-is left the
  narrower thing's bytes where the alternative's tag and payload belong, and the variant read back as **neither**
  alternative - no error anywhere, just a wrong answer.
- `CoerceStructurally` rebuilds a value as the same class at a different instantiation, attribute by attribute,
  widening the ones whose type changed. Each side maps its own declaration index through its own `FieldIndexMap`. It
  gives up (answering null, leaving the caller with what it had) rather than ever storing a value whose lowered type is
  not the field's.
- `ObjectInitializerAst::Stage11` coerces each argument into its attribute's type. Every other by-value position did;
  this one stored straight into the field.

**Do not** restrict the top-level coercion to variant targets only "for symmetry" - an early attempt engaged it for a
borrow against a value (`&StrView` vs `StrView`, which `TypeEq` also calls unequal) and rebuilt a struct where a
pointer was wanted, producing invalid ir in `string_view.spp`, `single.spp` and `ops/idx.spp`.

**The closure site, same shape, also fixed.** A closure's type is its body's, so `o.and_then((t: T) Some(val=..))`
has a `Some[U]` where the constraint wants an `Opt[U]`. A closure reached through a generic is called by the signature
its constraint promised (`sym->IndirectFnType`), while the function it points at was compiled with its own - so the
call read back a value that was never written and the chain quietly answered the `unwrap_or` default. The closure-call
path in `PostfixExpressionOperatorFunctionCallAst::Stage11` now takes the return type from the lhs's *concrete*
functional type (`FindFnSup(..)`'s `Out` argument, not `IndirectFnType`), calls through that, and widens the
result to what the call site expects. `std::net`'s socket timeouts are back to the `and_then` chain. Test:
`regression::tst::codegen::test_a_closure_returning_an_alternative_widens_into_the_variant_asked_for`.

Related: [[spp-generic-inference-is-one-way]], `net-module-findings` (SPP-STL notes), `check-variant-is-single-level`.
