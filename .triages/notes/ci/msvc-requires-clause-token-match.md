---
name: msvc-requires-clause-token-match
description: "MSVC C3864 \\\"requires clause is incompatible with the declaration\\\" = forward decl and definition spell the constraint differently"
updated: 2026-09-11
---

MSVC (windows-2025, VS 18 / 14.51, 2026-09-11) rejects a constrained class template whose forward declaration and
definition spell the requires-clause with different tokens, even when both name the same entity - e.g.
`derived_from<T, SemanticError>` inside the namespace vs `derived_from<T, spp::analyse::errors::SemanticError>` on the
out-of-namespace `struct spp::analyse::errors::SemanticErrorBuilder` definition. The standard agrees (equivalence is
ODR token-equivalence); GCC and Clang just don't diagnose it.

**How to apply:** qualify the *forward declaration* to match the definition - the definition's template-head is
looked up at global scope, so it cannot drop the qualification. Only `semantic_error_builder.ixx` had such a pair when
fixed. Related: [[msvc-gmf-inline-bodies-in-importers]].
