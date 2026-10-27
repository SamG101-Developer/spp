---
name: spelled-compares-ok-off-hot-path
description: "One-place name comparisons (\\\"self\\\", \\\"T\\\", \\\"w\\\", \\\"n\\\") are fine; only replace spelling with identity when architectural or hot"
updated: 2026-10-13
---

A name comparison at a single site (regions.cpp `"self"`, `TypeArgRef("T")`/`CompArgId("w")`) stays as is. N2.1 and
N2.3 in .triages/next-steps.md were struck through for this (2026-10-13).

**Why:** the maintainer judged flags/handles/ParamId plumbing not worth it when the spelling is local and not on a hot path or
an architectural seam.

**How to apply:** for "spelling where there should be identity" items, first ask if the compare is one place and
off the hot path; if so, propose striking it rather than designing a fix. Fix it when user code can collide with the
spelling (as `$` could, N2.2) or the compare is spread across many sites. See [[switch-impls-off-typeast]].
