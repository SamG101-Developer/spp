---
name: exhaust-scope-current-scope
description: "ExhaustScope used to leave CurrentScope on the deepest descendant, so Exhaust+MoveOut landed inside the skipped scope; fixed 2026-09-28. Plus the folded-expression scope pattern (array repeat Size)"
---

**The bug:** `ScopeManager::ExhaustScope()` advanced the walk to `GetFinalChildScope()` and left `CurrentScope` there. So
the ubiquitous `ExhaustScope(); MoveOutOfCurrentScope();` pair only went up one level, which lands *inside* the
skipped scope whenever that scope has nested children.
- **Masking:** the next `MoveToNextScope` overwrites `CurrentScope`, which hid it.
- **When it bites:** something reads or writes `CurrentScope` first. `let a = [..; case ..]` re-added `a` (taken out
  by `_ExposePreviousSymbol` during value codegen) into the wrong scope. A later `let b = a` then got a null type and
  segfaulted in `TypeRef::Of`.

**The fix (2026-09-28):** `ExhaustScope` restores `CurrentScope` to the scope it exhausted, and the walk position is
unchanged. Callers:
- cmp statements (4 sites) and function/subroutine prototypes;
- the case-branch Stage 9 pattern skip;
- `ArrayLiteralRepeatedElementAst::SkipSizeScope`.

**Iterator-based exhaust (same day, later):** `ExhaustScope` now advances from the iterator's position (`*_It`), not
from `CurrentScope`. So "walk part of a scope's subtree, then skip the rest" works; comparing `CurrentScope` after
a partial walk and a `MoveOut` looped forever.

**Array-repeat size design (the proper fix, after the maintainer rejected a bin-op-only band-aid; see [[prefer-proper-fixes]]):**
- **Never analyse `Size` in place.** A non-literal size is analysed on a copy (`_AnalysedSize`) in its own
  `array-size` scope.
- **Templates keep the written size.** In a template (`Scope::IsInTemplate`: the scope chain holds an
  unbound `GenericParam` or `GnCompParam`), the written size is kept for each instance's clone to fold. The
  type is analysed inside the size scope, because a `case` as its comp arg opens scopes.
- **Elsewhere, fold the copy** with a `tm` reset to the size scope, and `Size` becomes the literal.
- **Build the `Arr[T, n]` type once** (`_ArrayType`). `InferType` returns it, and never re-analyses at codegen.
- **Stage 8** walks `_AnalysedSize`, then `ExhaustScope`. Stages 9 and 11 skip the size scope.
- **Verified:** a `case n == 1_uz { 2_uz } else { n }` size folds to `4_uz` in `f[4_uz]` and to `2_uz` in `f[1_uz]`.
- **Trade-off:** a never-instantiated template no longer reports a non-constant size.

**Abstract check gate:** `check_abstract` doesn't run from `kPreCodeGen` onwards, because `Scope::OnInstantiationMissing`
is off then and signature types can't be made. Kept as a guard.

**Renamed 2026-10-13 (N1.10):** `ExhaustScope` and `SkipPastScope` are now one `SkipSubtree(scope)`, which keeps this
behaviour (current scope stays on `scope`). See [[scope-manager-walk-api]].
