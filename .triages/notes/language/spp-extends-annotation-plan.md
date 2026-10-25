---
name: spp-extends-annotation-plan
description: "Planned !extends annotation (like Rust derive) will replace marker `sup X ext Copy { }` blocks; until then those blocks are fine"
---

The maintainer plans a `!extends(...)` class annotation, modelled on Rust's `#[derive(...)]`, to declare marker superimpositions
such as `Copy` on the class itself instead of as separate empty `sup X ext Copy { }` blocks (stated 2026-09-10).

**Why:** std has many empty marker-extension blocks (e.g. `sup Char ext Copy { }`, and now `sup NullptrErr ext Copy
{ }` / `sup Utf8Err ext Copy { }`), which are noise.

**How to apply:** For now `sup X ext Copy { }` is the accepted way to make a type `Copy` - write it without asking. Don't
build `!extends` or sweep std's marker blocks unprompted; it is later work the maintainer will ask for. When it is asked for, the
marker blocks across std are the cleanup target. Related: [[zero-type-implies-copy]], [[spp-linear-types]].
