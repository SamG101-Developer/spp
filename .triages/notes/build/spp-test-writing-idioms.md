---
name: spp-test-writing-idioms
description: "Syntax that costs a rebuild each time it is guessed wrong when writing s++ tests"
---

Collected while writing std unit tests — each of these was found by a failed build, so check here first.

- **Case patterns are destructure-only.** There is no bare `is T { .. }` and no `is T as n { .. }`. Matching an alternative of a variant is `is S32() { .. }` — the object-destructure form with an empty list. A class *with* attributes must list them or use the skip form: `is Vec[S32](..) { .. }`, else E47 "Argument Missing".
- **The subject is flow-narrowed inside the branch**, so use the subject's own name (`v`) to read the narrowed value; there is no binding to introduce.
- A nested `case ... of { .. }` **on one line** inside another branch is a syntax error. Split it into two statements.
- **Unary minus only applies to literals.** `-x` on a variable is a syntax error; write `0_s32 - x`.
- **Comparison-branch values are comma-separated:** `== b'\n', b'\r' { .. }`. `a | b` there is a *bitwise or* (it silently compared against 0x0f in `BufRead::read_line` and never stripped a line ending).
- `of` is a **keyword** — it cannot be a function name.
- **Everything non-`Copy` is linear**, including a `Var` holding a non-`Copy` alternative, `Pair`, `Vol` and `Single`. Every test binding one must end by consuming it (`drop(x)`, or a consuming method like `Mutex::take`).
- **`Vec` and `drop` are in scope unqualified** in std test modules; everything else needs its full path, and the path mirrors directories *plus filename* — `src/std/rc/rc.spp` is `std::rc::rc::Rc`, not `std::rc::Rc`.
- **Keywords that bite as identifiers:** `of` and `ext` cannot be variable or function names.
- **No escape sequences in string literals** — `\"` is a syntax error.
- `RawBuf` indexes to `&T or None`, not a plain borrow, so `buf[i]@` does not compile; use `take_at`.
- **`unwrap_or` evaluates its fallback eagerly** — it is an ordinary argument. Use **`unwrap_or_else`** with a closure when the fallback is expensive or can fault. A real bug came from this: `TimePoint::duration_since` fell back on an expression that underflowed and aborted in exactly the case where the fallback was not needed.
- Test groups: `!test(group="std/x/y")` in `tst/std/x/y.spp`. `spp test` takes **at most one** `-g`.

Related: [[spp-linear-types]], [[build-workflow]], [[add-tests-even-if-failing]].
