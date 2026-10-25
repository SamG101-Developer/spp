---
name: float-literal-bounds-exact
description: "A float literal is E16 if its exact written value is past the type's largest, even when it rounds to it (maintainer, 2026-10-22)"
updated: 2026-10-22
---

Float literal range checks (E16, `FloatLiteralAst::ValidateBounds`) compare the exact written decimal against the exact largest finite value: `65505.0_f16` is an error although it rounds to 65504. The maintainer chose this over Rust's rule (reject only what rounds to infinity) on 2026-10-22: "more exacting".

**Why:** the maintainer prefers the stricter check.

**How to apply:** don't relax bounds to rounding. Anything the compiler writes back as a literal must not exceed the exact bound: `utils::numbers::WriteFloat` writes integers exactly (the shortest form of `f32`'s max reads past it) and fractions shortest. Bounds come from `APFloat::getLargest` written exactly, never via the shortest form. Related: [[bigdec-tostring-is-rational]].
