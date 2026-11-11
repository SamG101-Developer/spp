---
name: msvc-exported-inline-vars
description: "MSVC may emit no definition for an exported mutable inline variable (SPP_EXP_CMP); declare it extern, define it in the .cpp"
updated: 2026-11-07
---

MSVC (19.51-19.52) can link `export inline` (SPP_EXP_CMP) variables as `lld-link: error: <root>: undefined symbol`:
no object file holds the definition. It first hit variables with a dynamic initialiser (kPrelude, kBuiltinFuncs,
TEMPLATE_SYMBOLS, fixed 2026-10-25), and the rule then was "constant-initialised is fine". That rule is wrong:
`stats::Counts` (`auto Counts = Arr<std::uint64_t, N>{}`, zero-initialised, so constant-initialised) failed the same way
on 2026-11-07, and was fixed with the same shape. Why the `Shared<TypeAst> X = nullptr` ones in
`common_types_precompiled.ixx` still link is unknown (one difference: they name their type, `Counts` was `auto`).

**Why:** GCC/clang emit these definitions, so only the Windows link breaks, and usually only once the rest of the
MSVC build compiles, long after the variable was added.

**How to apply:** never add a mutable SPP_EXP_CMP variable; use `SPP_EXP_FUN extern T X;` in the .ixx and the definition
in the module's .cpp. Inline functions in the .ixx can still use it. `constexpr` SPP_EXP_CMP constants (kCounterCount) have not hit it.
Related: [[msvc-gmf-inline-bodies-in-importers]].
