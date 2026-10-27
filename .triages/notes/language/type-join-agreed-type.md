---
name: type-join-agreed-type
description: "case branches, loop exits (+else) and an inferred closure's rets agree on one type via type_join::TypeJoin, by the case rule (the maintainer chose it, 2026-10-24)"
---

N9.1 (2026-10-24): `analyse::utils::type_join::TypeJoin` is the one rule for a type many values agree on. The maintainer chose
the `case` rule for all three sites: the target (what the whole is assigned to) if any; else the widest variant among
the values; else the first value that is not `!`. Every other value must be `Assignable` into it (E1 otherwise). A
value on a diverging path is no value; when every value diverges the `!`-typed ones stand for it and the result is
marked `Diverges` (a loop is then `!`).

Sites: `case_utils::JoinBranches` (case), `meta->LoopJoins` by depth (each `exit` gives; the loop gives its `else`
when it has exits, then solves; a loop with no exit is still `Void`), `meta->ClosureRetJoin` (an inferred `fun`
closure's `ret`s give; the body's own value too when it can finish; a `ret` keeps a null `_RetType`, which
`EmitReturn` reads as the closure's agreed type). `InstantiateFnValues` instantiates a function named by any value.

**Why:** three rules disagreed (`exit Some(..)` then `exit opt` was E1 in a loop but fine in a `case`).

**How to apply:** a new "many values, one type" site gives to a `TypeJoin`; don't add a first-value-wins rule.
A `cor` closure's `gen`s give too and resolve against `Gen[agreed]` afterwards (`GenExpressionAst::ResolveYield`);
unreachable until coroutine closures stop being refused (E100), so it has only been probed with the refusal lifted.
Related: [[codegen-coercion-sites]].
