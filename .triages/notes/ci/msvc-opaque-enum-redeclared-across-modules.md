---
name: msvc-opaque-enum-redeclared-across-modules
description: "MSVC C1117 'symbol X has already been defined': an interface forward-declared an enum it also imports; use use_alias, not use(…, enum class X)"
updated: 2026-11-07
---

`use(ns, enum class X)` expands to an opaque enum declaration, `namespace ns { enum class X; }`, attached to the module
that writes it. When that module also imports the module defining `X`, MSVC 14.51 counts both as definitions. Any
file that then names `X` stops with `fatal error C1117: unrecoverable error importing module '<defining module>':
symbol 'X' has already been defined`. GCC and clang accept it. a55be4f4 added four of these (`symbols.ixx`:
ConventionTag, Visibility; `pattern_solver.ixx`: SubjectOnMatch; `monomorphization.ixx`: CompilerStage). They were
changed to `use_alias` on 2026-11-07.

**Why:** forward-declaring an enum is only needed when its module is not imported, and the duplicate breaks only the
Windows MSVC lane.

**How to apply:** if a file imports the module that defines an enum, bring the name in with `use_alias(ns, X)`. Use
`use(ns, enum class X …)` only without that import, with the definition's exact underlying type: `type_identifier_ast.ixx`
still declares `CompilerStage : std::uint8_t`, which does not match the definition. Related:
[[msvc-interface-names-not-inherited]].
