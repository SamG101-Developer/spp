---
name: tuple-sup-matching-broken
description: "History: tuple sups were broken because a tuple's arguments stayed positional; since pack-design phase 11 a tuple is an ordinary class over a pack, and the special case is gone"
updated: 2026-11-04
---

**Superseded 2026-11-04 (pack-design phase 11).** Until then a tuple was the only type whose generic arguments stayed
positional: naming them would fold the pack back into the tuple being named (`Tup[Items=Tup[...]]`), so its pack was
left unbound and every by-name path (instance making, alias resolution, owner generic merging, arity checks) needed a
tuple branch. The 2026-08-25 fixes patched four of those sites.

Now a type pack is bound to a list, not a tuple, so `Tup` binds `Items` like any class and none of those branches
exist ([[variadic-pack-model]]). The tests in `tests/compiler/test_msc_tuple_sups.cpp` all pass, including the two that
were red here: `test_tuple_superimposition_variadic_empty_tuple` and `test_tuple_superimposition_param_name_shared_
with_stdlib`.

**Why:** a fix that special-cases one class's arguments leaks into every path that reads arguments by name; the cure
was to remove the reason the class was special.

**How to apply:** if a tuple (or variant) behaves differently from another class over a pack, treat it as a bug in the
pack model, not a place for a tuple branch.
