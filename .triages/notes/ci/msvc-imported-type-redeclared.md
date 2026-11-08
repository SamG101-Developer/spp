---
name: msvc-imported-type-redeclared
description: "MSVC: use(ns, struct/enum X) in a file that imports X's module makes a second, empty X (C1117 for enums, C2027 for structs); use use_alias"
updated: 2026-11-08
---

`use(ns, struct X)` / `use(ns, enum class X)` expands to a forward declaration, `namespace ns { struct X; }`, attached
to the module that writes it. When that module also imports the module that defines `X`, MSVC 14.51 keeps the two
apart. GCC and clang accept it. MSVC fails in one of two ways:

- **enum:** `fatal error C1117: unrecoverable error importing module '<defining module>': symbol 'X' has already been
  defined`, in any file that names `X` (`ConventionTag` from `symbols.ixx`).
- **struct/class:** a type that importers see through the interface is the never-defined forward declaration:
  `C2027: use of undefined type 'spp::codegen::LlvmFnWrapper'` on `GetLlvmFn()->Target` in
  `subroutine_prototype_ast.cpp`, from `function_prototype_ast.ixx`'s `use(spp::codegen, struct LlvmFnWrapper)`.

a55be4f4 added 25 of these (4 enums, 21 structs); 16 more were older and had not yet failed. All were changed to
`use_alias` on 2026-11-07/08.

**Why:** each one fails only the Windows MSVC lane, in whatever file first needs the full type, one CI run at a time.

**How to apply:** if a file imports the module that defines a type, bring the name in with `use_alias(ns, X)`. Use
`use(ns, struct X)` only without that import (to avoid a module cycle or a heavy import), and for an enum with the
definition's exact underlying type: `type_identifier_ast.ixx` still declares `CompilerStage : std::uint8_t`, which
does not match the definition. A `use(ns, struct X)` in a file whose imports already give a global `using ns::X` (another file's `use_alias`) ICEs a checking GCC 16 (`finalize_dependencies`, module.cc:16082, `function_implementation_ast.ixx`): import X's module and `use_alias` it there too. Related: [[msvc-interface-names-not-inherited]].
