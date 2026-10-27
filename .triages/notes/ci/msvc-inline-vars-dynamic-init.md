---
name: msvc-inline-vars-dynamic-init
description: "MSVC emits no definition for an exported inline variable with a dynamic initialiser; declare extern, define in the .cpp"
---

MSVC (19.52) links `export inline` (SPP_EXP_CMP) variables with a dynamic initialiser (Str from a literal, a Map, a function call result) as `lld-link: error: <root>: undefined symbol`. Constant-initialised ones (`Shared<T> X = nullptr`) are fine. Fixed 2026-10-25 for kPrelude, kBuiltinFuncs, TEMPLATE_SYMBOLS: `SPP_EXP_FUN extern ...;` in the .ixx, definition in the module's .cpp.

**Why:** GCC/clang emit them, so only the Windows link breaks.
**How to apply:** never add a new SPP_EXP_CMP variable that is not constant-initialised; use the extern + .cpp shape. Related: [[msvc-gmf-inline-bodies-in-importers]].
