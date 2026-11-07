---
name: apfloat-tostring-not-rounded
description: "llvm::APFloat::toString computes all a value's exact digits whatever the precision, and its digits aren't correctly rounded; WriteFloat calls it once and rounds the text"
updated: 2026-11-02
---

`llvm::APFloat::toString(buffer, digits, ...)` has two traits that matter to `utils::numbers::WriteFloat`:

- **Its cost does not fall with `digits`.** It builds the exact value first: `5^16445` for the smallest `f128`, for one
  digit as for forty.
- **Its digits are not correctly rounded.** It truncates the significand before printing, so `f32` 81745.265625 at 8
  digits gave `81745.265`, and a search over digit counts could miss a shorter text that reads back (`0.00000739` where
  `0.0000074` reads back too).

`WriteFloat` used to call it once per try of its digit search, about 6 times per float, which was about 4% of a gtest
process (std's `limits.spp` constants, through `ValueKeyOf`). It now writes 40 digits once and rounds that text itself
(`RoundDecimal`).

**Why:** 2026-11-02. Checked against the old algorithm over about 760,000 values (every `f16`, random bit patterns,
short decimals, nines, limits, for `f16`/`f32`/`f64`/`f128`): every text that changed reads back to identical bits and
is no longer than before, and integers never change. About 2% of those values changed text, most of them shorter.

**How to apply:** a float's text is part of its comp identity, so it can show up in mangled names
([[mangled-names-may-change]]); a one-text-per-value invariant is what must hold, not the exact digits. Don't compare
`WriteFloat` against `toString` at the same digit count, and don't add a `toString` call per candidate to a search over
digits. Related: [[spp-perf-hot-path]].
