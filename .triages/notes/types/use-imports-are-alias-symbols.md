---
name: use-imports-are-alias-symbols
description: "A `use` of a class makes an Alias-kind symbol; TypeRef resolves it away (AliasTarget, 2026-09-28) but raw FindTypeSymbol does not, and Class-only stamp rules skip it"
updated: 2026-09-15
---

In s++ a `use std::x::Foo` creates a `TypeKind::Alias` symbol (with `AliasInfo`, `IsFromUseStmt`) in the importing
scope, so a plain name written there resolves to the alias, not the class. Any rule that stamps or accepts only
`TypeKind::Cls` (Stage7's plain-name stamp in `TypeIdentifierAst`, and `type_resolution::StampWrittenParts` before
2026-09-15) leaves such names unstamped; they are then resolved by spelling wherever they are read, and fail in scopes
without the `use` (E26 "GlobalAlloc" inside `BufRead[R=R]`, E78 on `Zero` from `string_view.spp`).

**Why:** every "re-qualify into `FqName`" site was quietly going *through* the alias (an alias's `FqName` is its
resolved target). Removing one breaks std only where a `use`-imported name reaches another module. It took two
instrumented probes to find, in design change 1.3b (`.triages/docs_closed/generic-design-roadmap.md`).

**Since 2026-09-28 (audit F4):** `TypeRef::Of`/`OfHead` never answer with an alias. Following an alias is one rule,
`TypeSymbol::AliasTarget(scope)`: the alias's linked scope, else `DeclScope`, else the asking scope, followed to a fixed
point. Raw `Scope::FindTypeSymbol` still answers with the alias, which visibility checks and argument naming need.

**How to apply:** follow an alias with `AliasTarget`, never by hand-reading `Alias->Resolved`. Don't make Stage7's rule follow aliases blindly: that changes how every
`use`-imported name resolves (visibility, alias-aware callers). Symptom: E26/E78 naming a type the failing file never
imports. See [[resolved-types-generic-identity]].
