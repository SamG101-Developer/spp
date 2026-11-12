---
name: spp-test-writing-idioms
description: "Syntax that costs a rebuild each time it is guessed wrong when writing s++ tests or benchmarks"
updated: 2026-11-11
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

Found writing the run-time benchmarks (`benchmarks/runtime`, 2026-11-02), for a project rather than the std:

- **`Vec` is in the prelude for a project too**: `use std::vector::Vec` is E14 (duplicate). `Range` and `Copy` are not.
- **The array type is `std::array::Arr[T, n_uz]`**; `[T; n]` is only the literal (`[0_s64; 16_uz]`), and as a type it
  is a syntax error.
- **A closure capturing by value (`caps k`) can be called once**: the second call is "uninitialized memory used". Capture
  by borrow (`caps &k`, read `k@`) to call it in a loop.
- **Passing a `&mut` parameter on to a call uses it up**: a second call with it is "uninitialized memory used". Pass it
  to one callee, or have the owner make each borrow (`f(&mut v)` twice).
- **Print with the formatted `println`** (std `3af56ae`, 2026-11-03): `use std::console::println`, then
  `println("{}\n{}", a, b)`, as the Rust versions do. The old per-type `show`/`show_s` helpers are gone (a generic
  `show[T]` could not call `Str::from(x)`: no `From` impl is chosen for an unbound `T`).
- **There is no argv and no integer parsing**; `std::process::get_env` returns an unparsed `Str`. For an input the
  optimiser must not see, use `std::hints::black_box` (`use std::hints::black_box`), as Rust's `black_box`. It
  replaced each benchmark's `hide()` (a volatile round-trip).
- **Methods in a `sup` block are private by default**: mark each one called from outside `!public`. An override in
  `sup X ext Base` must not be: it takes the base method's visibility, and `!public` on it is "invalid target of
  annotation".
- **`None` is a zero type**: `ret None`, not `ret None()`. `Some(val=x)`, `Pass(val=x)`, `Fail(err=x)`; match with
  `is Some[U64](val)`, `is Pass[U64](val)`.
- **There is no dynamic dispatch through a base type**: an override is called on the derived value. An indirect call is a
  function value (`FunRef[(U64,), U64]`) passed to a `!no_inline` function (`use std::llvm::no_inline`).
- `v[mut i]@ = x` writes an element; `v[i]@` reads one. `F64::from(u)`, `S64::from(f)` (truncating) and
  `USize::from(u64)` convert.

**C++ unit tests that lex:** call `lexer.Lex()` before `lexer.PreludeTokenIndex()`, in separate statements, never
both as arguments of one call (`ForFile(lexer.Lex(), path, lexer.PreludeTokenIndex())`): C++ does not fix the order
arguments are evaluated in, and GCC reads the index first, before lexing has set it, so the prelude reads as the
author's code (2026-11-11, `test_msc_source_positions.cpp`, `test_msc_error_files.cpp`).

Related: [[spp-linear-types]], [[build-workflow]], [[add-tests-even-if-failing]].
