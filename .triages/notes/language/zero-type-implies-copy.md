---
name: zero-type-implies-copy
description: "!zero_type does NOT imply Copy - that rule was reversed on 2026-08-26; a marker type is linear unless it is Copy"
---

**Reversed.** `!zero_type` used to make a type implicitly `Copy` (a 2026-07-28 decision). Commit `535ce04f`
(2026-08-26, "Isolate, remove `zero_type` fallthrough") took that out of `TypeSymbol::IsCopyable`, and `9ba333e3`
(2026-08-28) pinned the new rule with `TestLinearExemptions.test_invalid_zero_type_is_not_copy`: being zero-sized is
a statement about layout, not about what an assignment means, so a marker like `NullptrErr` or `AllocOomErr` still
has to be consumed. `!zero_type` now only guarantees no storage (an attribute on one is `SppEmptyBodyRequiredError`).

**Why it matters:** I trusted the old version of this note on 2026-09-10 and briefly told the user a function
parameter of type `NullptrErr` being reported unconsumed was a compiler bug - it is the intended behaviour. To discard
a marker, destructure it (`let NullptrErr() = e`) or pass it to `std::mem::ops::drop`.

Related: [[spp-linear-types]].
