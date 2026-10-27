---
name: getallparams-includes-self
description: "GetAllParams()[0] on a method is the self parameter, not the first written-out one; this silently broke NonNull::write and Slot::replace"
updated: 2026-09-01
---

`FunctionParameterGroupAst::GetAllParams()` returns `Params` verbatim, and the **self parameter is in `Params`** (`GetSelfParam()` filters it out by `dynamic_cast`). So in an intrinsic's llvm impl, `proto->FnParamGroup->GetAllParams()[0]` is `self` on any method, not the first written-out argument. Use:

```cpp
proto->FnParamGroup->GetAllParams()[proto->FnParamGroup->GetSelfParam() != nullptr ? 1 : 0]
```

Better still, use **`GetNonSelfParams()[0]`** — it returns `Vec<FunctionParameterAst*>` (a complete type in that TU) and filters self out for you. (`GetRequiredParams()` looks right but its element type is incomplete there.)

**Why:** this was one bug behind three symptoms, found 2026-09-02. `StdNonNullWrite` and `std_slot_replace` both loaded index 0 as the value being stored, so they wrote the *pointer/receiver* over the thing it pointed at. That made `NonNull::write` store garbage, which made every `Single` and `Rc` read back nonsense (both build their contents with exactly one `write`), and made `Slot::set` a no-op. Fixing the index turned 13 red tests green in one go.

**How to apply:** when an intrinsic that stores or returns a *written-out argument* misbehaves while the same type's read paths work, check this indexing first. **This keeps recurring** — fixed at five sites so far: `StdNonNullWrite`, `std_slot_replace`, `StdVolWrite`, `StdVolReplace`, and `SimpleAtomicFetchRmw` (which loaded the atomic's own address as the operand, so `exchange`/`fetch_and`/`fetch_or`/`fetch_xor` all returned nonsense). In each case the comment above the line already said "the value parameter" or "the only non-self parameter" — the *intent* was right and the index was wrong, so grep for the pattern rather than trusting the comments. Ten `GetAllParams()[0]`/`Params[0]` sites remain in `llvm_fn_impls.cpp` and have not been audited; each is only correct if its function has no `self`.

A second bug found alongside it, now also fixed: **forwarding coercion was not applied at a `gen`**. `TypeFwdEq` accepted `gen self` in a coroutine whose yield type the receiver only reaches by forwarding, but nothing rewrote the expression, so the generator yielded the forwarder itself. `GenExpressionAst::Stage7` now does the `BuildFwdCall` rewrite that `overload_utils` does in argument position, and `BuildFwdCall` picks `fwd_mut`/`fwd_ref` from the receiver's convention instead of always preferring `fwd_ref`. That let `std::single` go back to its original `gen self` with no diff.
