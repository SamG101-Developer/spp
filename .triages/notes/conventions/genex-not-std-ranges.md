---
name: genex-not-std-ranges
description: "Range algorithms use genex (genex::actions / genex::algorithms / genex::views), not std::ranges"
updated: 2026-10-25
---

Write range algorithms with genex (`genex::actions::replace`, `genex::contains`, `genex::views::...`), not
`std::ranges::*`, in both compiler sources and tests. A file that starts using it needs `import genex;`.

**Why:** the maintainer replaced `std::ranges::replace`/`std::ranges::contains` with genex in a review (2026-10-25);
genex is the codebase's one range library.

**How to apply:** reach for genex first; see [[genex-view-gotchas]] for its traps.
