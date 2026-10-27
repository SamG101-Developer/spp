---
name: comp-key-folds-by-intrinsics
description: Comp keys fold operators through the intrinsics' comp-time impls (FoldBinaryOp); one arithmetic, two entry points
updated: 2026-10-26
---

Comp values are folded in two places, and both now run the same arithmetic:

- **Keys** (`scopes::FoldCompValues`, called by `RawCompKey` and `RewriteCompId`) fold `A[1 + 2]` to the identity
  `A[3]`. This runs from Stage 2 on, with no scope manager and no ASTs to resolve (`RewriteCompId` works on keys
  alone), so it can't resolve `1.add(2)`. It keeps only the key layer's own rules: types don't mix, an unsuffixed side
  takes the other side's type, a shift is typed by its value. Then it hands the operation to
  `comp_time_intrinsics::FoldBinaryOp`.
- **Calls** (a `cmp` value, a fold of an analysed expression) go `+` → `S32::add` → `std::intrinsics::sadd` → its
  `CompTimeImpl`, through `FunctionImplementationLoweredAst::EvaluateCompTime`.

`FoldBinaryOp` is a table from each operator to the intrinsic that std's method for it calls (`kBinaryOps`, with int,
float and Bool columns). `and`/`or` are built in, not methods. It refuses exactly what the call raises
(`IsZeroDivisor` → E90, `IsShiftTooWide` → E91), and `ValueKeyOf` refuses a non-finite float, which the call raises
as E16. A refused key stays an `Op` key. `comp_generics::FoldCompCall` then folds that operator through the call, so
the error is raised: `A[7_uz / 0_uz]` gives E90, not an opaque type's E1. `ValueKeyOf`/`LiteralOf` are the one
literal-to-key conversion, used by `RawCompKey` and `CompAstOf` too.

Before 2026-10-25 the key layer had its own if-chains, which disagreed with the calls:
- a zero divisor and a too-wide shift (the key left the first unfolded and wrapped the second to 0);
- float `%` (the key didn't fold it);
- Bool `& | ^` (the key didn't fold them).

The calls ICE'd on Bool `== != & | ^ not`, because std's `Bool` methods call integer-typed intrinsics. Those
intrinsics now take either kind.

**Why:** two implementations of one arithmetic drift, and the type a value keys as must be the value a call folds to.

**How to apply:** a new comp-time operator or intrinsic goes in `kBinaryOps` and its `CompTimeImpl`, never in
`FoldCompValues`. A new check that raises at fold time needs a predicate the table also uses to refuse. Probe both paths
with `A[<expr>]` against `A[v()]` where `cmp fun v() -> T { ret <expr> }`.
