---
name: split-functions-at-natural-seams
description: Long functions split at natural seams, no line target; helper goes private if one AST uses it, else into a utils/codegen module
updated: 2026-10-26
---

How the maintainer wants long functions split (N7.6, 2026-10-26):
- **Seams, not sizes.** Split along the sections the function already has, usually its comment blocks. There is no line
  target.
- **Placement by use.** A piece only one AST uses becomes a private member of that AST (`_Name`, like `_CheckVisibility`
  and `_Resolve`). A piece with more than one user goes in a helper module: `analyse/utils` for analysis, the `codegen`
  modules for emission. A new module file is fine when none fits.
- **Deduplicate while splitting.** Compare sibling functions first (the two loops' Stage 8, the class and function
  instantiation makers), so shared structure becomes one function rather than two copies split the same way.

**Why:** the aim is a smaller, simpler compiler ([[code-reduction-means-functions]]). A split that only moves text
around adds names without removing anything.

**How to apply:** don't lift single-use code into a utils module, and don't inline a helper into a lambda in its
caller. Verify a split as a pure move: a build and probes, plus an IR diff of `./project` for codegen
([[spp-ir-diff-verification]]).
