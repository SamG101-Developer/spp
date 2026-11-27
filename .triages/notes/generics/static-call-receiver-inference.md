---
name: static-call-receiver-inference
description: "Vec::from(&[1, 2]) infers Vec[S32]: the callee defers a bare template receiver, the call solves each block's method, writes the receiver and re-analyses"
updated: 2026-11-24
---

A static call on a generic class written without its arguments (`Vec::from(&[1_s32, 2_s32])`,
`Range::between(0_uz, 3_uz)`) infers the receiver from the call's arguments, in two phases inside the call:
1. **Deferral.** `PostfixExpressionAst::Stage7` marks a call's static-access callee (`MarkCalleeOfCall`). If that
   callee's receiver type raises E72 and was written with no arguments, it is kept as written and deferred
   (`LeaveReceiverToCall`), and the member check is skipped. The bare template only has its unconditional blocks attached,
   so `Vec::from` (in `sup [T: Clone] Vec[T]`) is not found on it.
2. **Inference.** The call analyses its arguments with no expected types, then
   `overload_resolution::InferTemplateReceiver`:
   - solves each `NormalSupBlocks` block's method of that name like a free function (`BindSlots`, `InferAllGns`),
     without instantiating anything. A method inherits its block's parameters, sharing their `ParamId`s, so the
     solution binds them.
   - drops a block on a recognised rejection, or on a concrete parameter its argument does not fit;
   - substitutes the solution into the block's pattern: that is the receiver. One distinct result is the receiver,
     more than one is E130, none leaves E72.
3. **Resolution.** `_InferStaticReceiver` writes that type into the callee, analyses the callee again, and the call
   resolves as if the receiver had been written. Attachment, constraints, instantiation and codegen are the normal
   path. With no result, the callee is analysed as written, no longer deferred, so its E72 is reported.

**Traps:**
- `ValidateArgsMatchParams` on the uninstantiated method rejects a closure given for an unbound `F` ("expected `F`,
  found `$closure`"). It is for an instantiated method, so only concrete parameters are checked here.
- Arguments are analysed with no expected types, as a normal call's generic parameters are, so closures still work.

**Why:** the receiver needs the arguments, and the arguments are normally analysed against the receiver's
candidates. Inferring first and then resolving normally keeps one resolution path.

**How to apply:**
- A solved receiver may name the enclosing function's own generics (`Vec::from(&[x])` in `fun f[U](x: U)`, whose
  body is analysed with `U` open); only one still naming the block's own parameters is skipped (2026-11-24: it used
  `IsConcreteTypeId`, which made every call inside a generic body E72).
- Not reached yet: partly written receivers (`Two[S32]::f`), `ext`-inherited methods (a Todo at
  `InferTemplateReceiver`), and blanket blocks.
- Tests: `TestGenericInference_StaticCall.*`. See [[generic-inference-engine-shape]],
  [[destructure-infers-from-subject]] (the same pick-one-candidate rule).
