---
name: checked-markers-are-sams
description: "Never add \\\"[CHECKED]\\\" comments (the maintainer's review tracking); removing, folding or rewriting a marked function is fine"
updated: 2026-10-14
---

Never write a `/// [CHECKED]` (or `// [CHECKED]`) marker on a function (2026-10-14,
N6.3). They track which functions the maintainer has reviewed by hand.

**Why:** they are the maintainer's personal management of the review, not documentation; adding one claims a review that did not
happen.

**How to apply:** the only rule is never to add one. A marked function can be removed, folded into its caller or
rewritten like any other (maintainer, 2026-10-16: "u can remove things that are [CHECKED], the only thing with that tag is
just never add it yourself"); keep the marker on a function that stays, and don't treat markers as stale comments in
hygiene sweeps.
