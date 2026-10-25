---
name: code-reduction-means-functions
description: "When asked to reduce/simplify code, remove duplicated or dead functions and widen reuse - not macro-generate boilerplate, nor inline single-use utils back into ASTs"
updated: 2026-10-25
---

"Reduce the code" in this project means removing actual functions: ones that are duplicated, dead, or can be refactored
so more call sites use one. Generating repetitive bodies with macros is not what the maintainer wants (2026-09-15, rejecting a
proposal to macro-generate the ~60 comp-time `std_intrinsics_*` functions in `comp_time_intrinsics.cpp`).

**Why:** the goal of the resolved-types refactors is a smaller, simpler compiler, especially the generic system; a
macro keeps every function and only hides the text.

**How to apply:** hunt for dead functions (no callers), near-duplicates (merge into one, as `Canon`/`CanonVar` became
`FindParamBinding`), and hand-inlined copies of an existing helper (as the stamp loop that became `StampCompGenerics`).
Don't propose macro generation as a reduction, and don't inline a single-use function into a lambda in its caller
either - it bloats that body and only moves the code (maintainer, 2026-09-15). The target is semantic analysis (`analyse/`, the ASTs' analysis
stages); leave general utilities alone even when members are unused - The maintainer kept the dead `Vec` members in
`utils/types.ixx` (2026-09-15). Related: `show-diff-before-scripted-edits`,
[[scope-symbol-family-duplication]], [[resolved-types-generic-identity]].

**Single-use is not dead (maintainer, 2026-10-25, N6.10).** The `analyse/utils` modules exist to de-bloat the AST
files, so an exported util with one caller - in an AST, or another module - stays exported. Inline only what is
truly redundant; a thin public name over a file-local `*Impl`, a cache clear, a type/comp twin or a named concept
is doing its job. Report the candidates before changing any.
