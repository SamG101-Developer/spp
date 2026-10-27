---
name: bigdec-tostring-is-rational
description: "numex::BigDec::ToString() returns the \\\"n/d\\\" rational form, not a decimal - use Decimal(places)"
updated: 2026-09-15
---

`numex::BigDec::ToString()` is `Base(10)`, which is `numerator + "/" + denominator` — so `1.25` stringifies as
`"5/4"`, **not** `"1.25"`. Only `BigInt::ToString()` gives plain digits. For a decimal string use
`BigDec::Decimal(max_places)` (default 64); `Point(n)` is the fixed-places variant. Source is at
`/usr/local/lib/cmake/NumEx/modules/numex/big_dec.cpp` — the class is header-and-impl in one module file, so read it
there rather than guessing from the name.

**Why it matters:** this silently destroyed every float literal in the compiler (2026-09-05). `FloatLiteralAst::
Stage11_CodeGen` did `llvm::APFloat(semantics, mapped_val.ToString())`; APFloat cannot parse `"5/4"` and leaves the
value at **zero**, so every `f32`/`f64` literal in the program lowered to `0.0` with no diagnostic at all.
`FromBigVal` had the same bug — it splits on `'.'`, so `"5/4"` became int-part `"5/4"`, breaking comp-time float
folding too. The failure is vicious because `0 op 0 == 0` is accidentally true: half the float tests still *passed*.
Only division (`0/0` = NaN), `!=`, `<` on a negative, `one()` and `pow(x, 0)` disagreed with zero and failed.

**Second trap - `BigDec(BigInt)` is `n/0`.** The constructor is `BigDec(BigInt const &num, BigInt const &den = {})`,
and a default `BigInt` is 0, so the one-argument form builds a zero denominator and throws "Division by zero!" from
`_Simplify` or the next operation. Every comp-time `floor`/`ceil`/`trunc`/`round` in `comp_time_intrinsics.cpp` did this and threw
from the day they were written; nothing tested them until `TestCompTimeValues.test_float_rounding` (2026-09-15). Always
pass `numex::BigInt(1)` explicitly. The `std::string` and `std::int64_t` constructors set the denominator themselves.

**How to apply:** never round-trip a value through `BigDec::ToString()` when something downstream will parse it as a
number. If the exact decimal is already in hand (a literal's own token text), pass that straight through instead of
going via `BigDec` at all — that is what `Stage11_CodeGen` does now. Where a `BigDec` genuinely must be printed, use
`Decimal()`, and size the budget by float width if exactness matters: f32 needs 149 fractional digits for its
smallest subnormal, f64 needs 1074, f128 needs 16494 (see `FloatLiteralAst::kDecimalPlaces`). Related:
`std-known-gaps` (SPP-STL notes), [[spp-ir-diff-verification]] — dumping `out/.../llvm/*.ll` and reading the `store double`
constants is what found this in one step.
