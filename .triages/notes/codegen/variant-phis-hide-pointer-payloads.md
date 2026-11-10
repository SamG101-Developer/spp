---
name: variant-phis-hide-pointer-payloads
description: "A phi of whole variants turns a reference payload into an integer (ptrtoint/inttoptr); build variants on several edges into one slot with StoreVariant"
updated: 2026-11-09
---

A variant lowers to `{ i64 tag, [N x i64] payload }`, so the payload cells are integers. Built into a slot with a
`ptr` store and only ever read back through that slot, SROA keeps the pointer a pointer. But a *whole* variant value
loaded on two edges and joined by a phi forces SROA to read the payload as `i64`: the pointer goes through `ptrtoint`
and back through `inttoptr` at the `case`, loses its provenance, and a loop over it never reduces to a counted loop or
vectorises. Found in `res()` (2026-11-09, O11): `iter_ref` was 8× C with the frame already elided; with the fix it is
at parity.

**Why:** `BuildVariant` allocates a slot, stores into it and loads the whole variant back, which is fine on one edge
and harmful when its results meet in a phi.

**How to apply:** where a variant is built on several edges and joined, allocate one slot (`LlvmEntryAlloca`), write
each edge with `StoreVariant`, and load once at the join (see `PostfixExpressionOperatorKeywordResAst`). Value payloads
(`Opt[U64]`) suffer far less. Two cases remain open, tracked as O13 in `../../docs_wip/llvm-optimisation-review.md`: `case`
used as an expression and loop exit values (fixable the same way), and variant-returning functions with several `ret`s
(LLVM's inliner builds that phi, so it needs a representation change). Check a hot loop for `ptrtoint`/`inttoptr`
before changing codegen.
