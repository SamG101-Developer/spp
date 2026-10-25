---
name: symmetry-levels-up
description: "When making type/comp (or any pair) symmetrical, raise the worse side to match the better one; never degrade the better side for symmetry's sake"
updated: 2026-10-02
---

When the maintainer asks for a pair to be "symmetrical and optimal" (type/comp helpers, keying arms, accessors), copy the better
design onto the weaker side. Do not make the efficient or cleaner side worse just so the two match.

**Why:** said 2026-10-02 during cleanup 3.1 ("make the worse one match the better one, not make the better one worse
for the sake of symmetry").

**How to apply:** before unifying, identify which side is better (fewer passes, by value vs out-params, cached vs
recomputed, clearer name) and say which in the report. Related: [[type-comp-symmetry-helpers]].
