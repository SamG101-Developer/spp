---
name: closure-copied-capture-is-fun-mov
description: "Any by-value capture makes a closure FunMov, copyable or not: a FunRef is Copy and never dropped, so a heap env would leak"
updated: 2026-11-23
---

`ClosureExpressionAst::_FnType` picks the kind from the captures: any by-value capture gives `FunMov`, else a `&mut`
one gives `FunMut`, else `FunRef`. A by-value capture of a **copyable** value (`|caps a|` with `a: U32`) still gives
`FunMov`, even though the body can neither consume nor change it. `ClosureExpressionCaptureAst::ConsumesCapture` (by-value
and not copyable) only decides whether the outer variable is moved (Stage8), not the kind.

**Why:** a by-value capture puts the env on the heap (`owns=1`), because the closure may leave the frame. `FunRef ext
Copy` in std (`function.spp`: "a `FunRef` owns nothing"), and `drop[T=FunRef[..]]` compiles to a no-op, so a copy-only
closure typed `FunRef` leaked its env wherever it was coerced to `FunRef`. This was tried on 2026-11-19 and reverted. It also
turned single calls (`let y = x()`) into E94 (under the linear rules of the time), since a `FunRef` call does not consume. The consuming `FunMov` call is
what frees the env.

**How to apply:** don't relax the kind for copied captures without first giving copy-only closures an env they don't
own (captures stored inline in the closure value, which `FunXXX`'s fixed `{fn, env}` layout rules out today).
`ClosureExpressionAst.test_invalid_closure_copied_capture_is_not_fun_ref` pins it. Related:
[[closure-env-ownership-header]], [[affine-ownership]].
