---
name: spp-generic-inference-is-one-way
description: "Generics are inferred from a thing's own arguments only, never from what its result flows into - so sometimes you must write them"
updated: 2026-11-19
---

**The rule (by design, confirmed by the user 2026-09-06).** Inference runs **one way**, left to right and top to
bottom over a full declaration. A thing's generic arguments are read off **that thing's own arguments** - a function
call's arguments, an object initializer's arguments - and nothing else. It does not reach backwards from what the
result is later used as, so a return type, an assignment target or an enclosing variant never drives it.

The consequence is that a generic sometimes has to be written even though a reader can see what it "obviously" is:

```
# Wrong: "T" is read off "val", so this is a "Pass[None]" whatever the function returns.
ret Pass(val=None())

# Right: name the argument.
ret Pass[Opt[Duration]](val=None())
```

`std::net::tcp::linger` is written that way for exactly this reason. Do not treat this as a bug to fix, and do not
write a red test asserting the inferred form works - it encodes semantics the language does not have. (An earlier pass
did both; the test was removed.)

**A written generic is a preference, not a requirement for correctness.** Both spellings have to lower the same way -
a shape the analyser accepts cannot be one code generation quietly mishandles. `ret Pass(val=Some(..))` returning a
`Res[Opt[T], E]` now works, because the value is rebuilt as the alternative it matched; see
[[spp-variant-widening-in-generic-args]].

**A destructure's subject is its argument** (maintainer, 2026-11-19): `let Box(v) = b` and `case o is Some(val)` read
their unwritten arguments off the value being taken apart, which flows into the pattern as a call's argument flows
into the call, so this is still one way. See [[destructure-infers-from-subject]].

Related: [[spp-variant-widening-in-generic-args]], [[closure-return-types]].
