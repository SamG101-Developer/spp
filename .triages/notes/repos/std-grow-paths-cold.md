---
name: std-grow-paths-cold
description: SPP-STL's collection growth is out of line (!cold !no_inline) and amortised; the fits-already test is all that inlines
updated: 2026-11-09
---

In SPP-STL, the path that grows a buffer is a separate `!cold !no_inline` function (`Vec.grow_one`,
`RawBuf.try_capacity_grow_amortised`), and the public entry points (`Vec.append`, `RawBuf.try_capacity_reserve`,
so `capacity_reserve`, `append_from_view` and every `Str` append) do only the "already fits" test before calling it.
Growth is amortised: at least double the capacity (`max(len + additional, 2 * cap)`), never `len + additional`.

**Why:** with the grow path inlined, `append` was too big to inline, so a loop's `Vec` escaped into it and kept its
`sppc_malloc`/`sppc_free` pair even with the allocator attributes ([[runtime-allocator-attrs]]); out of line,
`codegen/drops` fell from 508 ms to 120 ms (C 109, Rust 119), 2026-11-09. Reserve used to grow to
`len + next_pow_of_two(additional)`, so `Str.append(char)` reallocated almost every call (quadratic). It is Rust's
shape too (`RawVec::grow_one` is `#[cold] #[inline(never)]`). `!hot`/`!cold` were parsed but not lowered until then.
**How to apply:** a new growing or rarely-taken path in a hot std method (reallocation, error formatting) goes in its
own `!cold !no_inline` function; keep the inlined part to the test. Std tests pin the growth rule
(`test_reserving_past_the_capacity_at_least_doubles_it`).
