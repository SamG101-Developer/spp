---
name: split-macro-headers
description: "Don't add macros to spp/macros.hpp - every TU includes it, so one line costs a full ~11 min rebuild; put them in a narrower header"
updated: 2026-09-18
---

`headers/spp/macros.hpp` is included by every translation unit, so changing a single macro in it
rebuilds the whole project — measured at 10m42s with `-j 6` on this machine. Put a new macro in the
narrowest header that needs it (e.g. `headers/spp/asts/reflect/macros.hpp` for the ast reflection
annotations) and include that from the files that use it.

**Why:** the user raised it unprompted after one such rebuild, and also wants the existing
`macros.hpp` / `macros-platforms.hpp` broken up the same way — the per-compiler GCC/Clang/MSVC bug
workaround macros are the example they gave, and the ast-only block (`SPP_AST_KEY_FUNCTIONS`,
`SPP_STRING_*`, `SPP_AST_KIND`, `SPP_AST_COMMON_FWD_DECL`) is another candidate.

**How to apply:** new macros go in a scoped header from the start. Splitting the existing ones is a
separate, scripted change — every ast `.ixx`/`.cpp` already includes `spp/macros.hpp`, so a move has
to add the narrower include to each of them; show the diff before running it
(`show-diff-before-scripted-edits`).

Related: [[spp-ast-reflection-refactor]], [[build-workflow]].
