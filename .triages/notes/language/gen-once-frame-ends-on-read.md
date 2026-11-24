---
name: gen-once-frame-ends-on-read
description: "A GenOnce frame finishes as soon as its value is read, so what it drops (a guard local) goes before the caller uses the yielded borrow; no scoped-resource coroutines"
updated: 2026-11-24
---

Reading a `GenOnce` (`let v = x.lock()`, or `x.fwd_mut()@ = ...`) resumes the frame, takes the yielded value and runs
the frame to completion there and then. Anything the frame drops then (a guard local, and before 2026-11-23 a `defer`,
which is gone) is released before the caller has used the borrow. A coroutine cannot hold a resource for the life of
the borrow it yields: `cor lock(&self) -> GenOnce[&mut T]` with an unlocking guard (then: `defer unlock`) hands out
`&mut T` with the mutex already released (checked 2026-11-15 by `trylock` from inside
the borrow's scope, which succeeded). Separately, `case v of` on a yielded `&mut T or Err` is E85 (moving a holder of
an escaping borrow).

**Why:** SPP-STL's `Mutex::lock` was this coroutine shape until SPP-STL #56. That replaced it with owned guards, which
could outlive the mutex and cross threads. Since 2026-11-15 the locks are closure-scoped (`with_lock`,
`with_read`/`with_write`).

**How to apply:** for scoped resources (locks, temporary state), take a closure and run it between acquire and
release; don't yield a borrow from a coroutine with cleanup after the `gen`. Revisit if `GenOnce` frames ever stay
suspended until the yielded borrow's scope ends.
