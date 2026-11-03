---
name: pack-constraints-pairwise
description: A type pack's constraints hold per element; one naming the pack itself (..Ts: Add[Ts]) holds for every pair; constraint inference skips packs
updated: 2026-11-04
---

A type pack's constraints hold for each element (`..Args: Format`), checked one by one in
`generic_inference::CheckTypeArgConstraints`. Since 2026-11-04, a constraint that names the pack itself
(`..Ts: Add[Ts]`) means "with every other element": each element is checked against the constraint read with the pack
bound to each element in turn, and `Self` bound to the element checked. A fold over a mixed pack is then proven at the
call, not inside the instance.

**Why:** pack-design phase 7. A binary fold `xs#i + xs#j` needs every pair to add. Plain `..Ts: Add` only says each
element adds with itself (`Rhs=Self`), which a mixed instance breaks inside the body.

**How to apply:**
- `_ReadConstraints` (inference through constraints) skips variadic parameters. A pack's value is its elements' tuple,
  which none of their constraints is matched against.
- `type_compare::UnmetConstraint` reads `Self` on both sides as the type checked when nothing matches as written,
  comparing by identity (open-bugs #53). It treats a hidden pack (`DerivesFromSymbol`) as the type it derives from.
- The check is n² per call; packs are short.

Related: [[variadic-pack-model]].
