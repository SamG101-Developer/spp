---
name: generator-gen-done-sentinel
description: "Resuming a finished Gen answers std::generator::GenDone, not None (2026-09-22); why nesting was rejected, E110, and the two latent codegen bugs it exposed"
---

`Gen::send` returns `Generated[Yield or GenDone]` (was `Yield or None`, which collided with `Opt` yields: variants
flatten+dedup, so `Opt[Str] or None` lost the "finished" case and ICE'd in `res` codegen). Done in `examples/vcs/std`
first (the maintainer merges to SPP-STL later) and mirrored into `tests/test_outputs/vcs/std` for the gtest suite;
`project/vcs/std` still says `None` until the merge.

**Why not nest (`Opt[Yield]`, the Rust way):** `Some[&T]` is E2 - classes cannot hold second-class borrows, and
borrow-yielding generators are common. Rust can nest only because borrows are first-class and enums don't flatten;
Rust's own coroutines use `CoroutineState::Complete`, i.e. a sentinel.

**Rules:** E110 `SppYieldTypeContainsGenDoneError` - a non-GenOnce yield type may not contain `GenDone`
(`marker_sups::EnforceYieldTypeWithoutGenDone`, called from the coroutine return type, `res`, and `loop in`; NOT from
`FindGenSup`, which the GenOnce-collapse check calls on every coroutine call). `?` on a `res` result no
longer has a `None` to early-return on.

**Latent bugs it exposed (both fixed):**
- `gen <member>` into a variant yield type stored the bare member with no tag (`GenExpressionAst` now calls
  `CoerceToVariant` for by-value yields).
- A case pattern naming a sub-variant (`is Opt[S32](..)` on `Some or None or GenDone`) found no single tag and emitted
  NO check - it always matched, silently. `codegen::NarrowVariant` now checks any-of and re-tags into the narrower
  variant. Only the identifier-subject path; the `self@`-style condition path still lacks it.

**Generators are lazy (2026-09-22, the maintainer confirmed intended; the eager comment was stale).** The coroutine ramp parks
at an initial suspend right after storing the parameters; `res` checks `coro.done`, resumes, checks again, then
reads the slot (never resumes a finished frame); `GenOnce` resumes then reads. The old eager ramp + read-then-resume
made every generator one value ahead, which deadlocked I/O-driven ones (`BufRead::lines` blocked reading line 2
before the caller had line 1).
