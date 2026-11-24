---
name: affine-ownership
description: "Ownership is affine since 2026-11-23: a non-Copy value moves at most once and is dropped by the compiler where its lifetime ends; where each drop point is computed and emitted"
updated: 2026-11-24
---

S++ was linear from 2026-08-26 to 2026-11-23 (every non-`Copy` value consumed exactly once, no implicit drops, E94).
It is now **affine**: a value that moves (`TypeRef::IsMoveOnly`: owned, not `Copy`, not `!`) is used at most once, and
whatever is still owned where its lifetime ends is dropped there by the compiler. Plan and history:
`.triages/docs_open/affine-migration.md`.

**Analysis decides, codegen emits.** Stage 8 records, on the ast that ends a lifetime, what is still owned there
(`drop_utils::OwnedValue`: the name, where it is declared, the parts already moved out), and Stage 11 drops exactly
that list (`codegen::EmitOwnedDrops` for a scope's own owners, `EmitOuterDrops` for values of enclosing scopes found
outward, `EmitScopeExit` for exits leaving several scopes, innermost first). All three live in `codegen/llvm_drop`. Every list is copied
by its ast's `Clone`, as some clones are generated without being analysed again.

| Drop point | Recorded by | On |
|---|---|---|
| Scope end (block, function / closure parameters, a closure's by-value captures, branch bindings) | `drop_utils::EndRegion` → `OwnedIn` | `_OwnedAtEnd` |
| `ret` / `?` (a lowered `ret`), `exit` / `skip` | `OwnedUpToFn`, `OwnedUpToLoop` | `_OwnedAtExit` |
| Generator frame destroyed at a `gen`, or before its first resume | `OwnedAtSuspend` | `_OwnedAtSuspend`, `_OwnedAtStart` |
| A lowered `GenOnce`'s by-value `gen` (its return) | the same list, passed to `EmitReturn` | `_OwnedAtSuspend` |
| Paths meeting that disagree (`case` branches and fall-through, loop exits) | `OwnedOnJoinPath` per path | `_DropsAtEnd` (a branch's, `AddDropsAtJoin`), `_DropsAfterExit` (an `exit`'s, `SetDropsAtJoin`), `_DropsOnFallThrough`, `_DropsOnCondEnd` |
| A guard's moves, on the edges passing its branch over (no match, guard false) | `case_utils::CarryGuardMoves` | `_DropsOnNoMatch`, `_DropsOnGuardFalse` |
| A logical operator's right operand vs its short circuit | `OwnedOnJoinPath` per path | `BinaryExpressionAst::_DropsOnShortCircuit`, `_DropsAfterRhs` |
| Overwrite (`x = v`, `p.a = v`, through `&mut`, `x@ = v`) | `OwnedOnPath`, `OwnedBeforeWrite` | `AssignmentStatementAst::_DropsOld` |
| Temporary borrowed, read a part off, or `case`d over without being taken | `IsTemporary` sets a flag | pending list, `codegen::PushTemporary` |
| A lowering's hidden `$` temporary (`$cmp`, `$spread`, `$fold`, `$_dst_`, `$is`) at its statement's end | `EndStatementTemporaries` | `_StatementDrops`, `_IterationDrops`, `_DropsOnCondEnd`, a `case`'s paths |
| Discarded temporary (a statement's value) | `expr_utils::DiscardValue` | `_DropsDiscarded` |
| A call fold's hoisted temporaries no call took | `OwnedOnPath`, then marked moved | `_FoldDrops` |

- **No drop flags.** A value held on only some of the paths that meet is dropped at the end of each path still holding
  it, before they meet; after the join it is held on none (`OwnedIn` skips `MaybeInit` / `MaybePartialMoves`). Earlier
  than Rust's scope-end drop, static. The loop head join records none: a value held there on some paths only cannot be
  used in the body without E8.
- **Only enclosing scopes' owners join.** `drop_utils::OwnersAround` starts at the `case`'s or loop's parent: their
  own scope holds `is` bindings that exist only where they matched, and the flow also holds branch-local symbols.
- **Pending temporaries** live on `ctx->Pending->Held`: a block statement drops what it pushed at its end, a
  `ret` everything since `meta->TemporaryBase` (set per function, coroutine, closure), an `exit` / `skip` since
  `LlvmLoopInfo::TemporaryDepth`, and a loop condition what it pushed once its value is made (each evaluation). A
  `case` over a temporary pushes it only inside branches that keep it.
- **Hidden `$` temporaries** (`VariableKind::Temporary`) end with the statement that made them, as `is` bindings do,
  except a statement that is itself the `let $x = ..` a lowering reads in the statements after it (`OwnLetName`). A
  loop condition's end every iteration, and on the path where the condition ends the loop; a `case` condition's are
  dropped on every path out.
- **A pattern guard may move** an outer value (2026-11-24): the move holds on every path past its branch, so the no-match
  edge (where the guard never ran) drops it, and later branches see it held on none (E8). E114 is left only for a guard
  moving the branch's own non-borrow bindings, which are still parts of the subject.
- **An `is` binding is a view** (2026-11-24): `o is Some(val as y)` binds `y` as a view of the part of `o` it names
  (`VariableKind::FlowNarrowing`, `VariableSymbol::ViewRoot`/`ViewSteps`, recorded from the pattern binding's source
  place). The subject's owner keeps owning it, so nothing drops an `is` binding; moving `y` moves that part out of `o`
  (`mem_utils::ValidateSymbolMemory`), the whole of it through a variant's payload. A subject that is no place is held
  by a hidden `$is` temporary to the statement's end. The lowered `case` never takes its subject. So there are no
  statement-end drops of `is` bindings at all (the old `kIsBindings` / `_DropsConditionBindings` are gone), and a
  name an `is` binding shadowed, from this scope or further out, is put back after the statement.
  `case o is P { .. }` is not this: it is rewritten to `case o of { is P { .. } }`, whose bindings own as any branch's.
- **A logical operator's right operand is a path of its own:** a move in it (`flag and take(x)`) is joined with the
  short circuit, which drops what it still holds.
- **A field read off a temporary** (`mk().a`) is moved out of it, the rest dropped as the statement ends; borrowed
  (`f(&mk().a)`, a `&self` receiver) or discarded, nothing is moved and the temporary is dropped whole
  (`ReadByBorrow`, `drop_utils::FieldReadOffTemporary`).
- **A statement `case` yields no value** (`ValueDiscarded`): codegen makes no phi and never joins its branch types.
- **A destructure takes the value apart** (choice A, 2026-11-24): its type's own `drop` never runs on it, and the
  owned parts the patterns skipped are dropped field by field by its owner (`MemoryState::TakenApart`, carried to codegen
  as `OwnedValue::TakenApart` paths). Binding every owned part moves it whole. So `let Self(..) = self` is how a
  `close`-style consumer disarms `drop` without leaking fields, and skipping an owned part of a `Drop` type is no E106.
  A field moved out by access (`let a = h.a`) is still E106. Paths that disagree about it are a "maybe", dropped on each
  path before they meet.
- **`self` at the end of its own type's `drop`** is dropped field by field (the empty path in `OwnedValue::TakenApart`),
  not through `drop` again. A partial move out of it is no E106 there.
- **Overwrites build the new value into a temporary** when an old one is dropped, never straight into the slot.
- **Shadowing keeps the old value** to its scope's end (`Scope::GetOwners` lists displaced bindings).
- **`std::mem::ops::drop` stays an intrinsic**: `drop_utils::EndsArgument` reads its marker to know the call ends its
  argument's loans. The lowered `loop x in ..` drops its iterator on every way out instead
  (`LoopConditionalExpressionAst::EndsWithLoop`), which ends its loan on the iterated value.
- **No `defer`** (removed 2026-11-23, plan phase G): clean-up that is not a drop is a guard value whose `Drop` runs it
  (`IterMovRest` in std's `Vec::iter_mov`).
- **E95 is for `!must_use` only** (`Fail`, `Fut` in std), or a variant with a must-use member (`Res`). E131: a `Copy` type cannot superimpose `Drop`.

**Why:** linear typing cost every `Str`, `Vec` and allocator explicit consumption (about 930 `drop` calls in std) to
protect a handful of resource types; the opt-in linear types for those are a future item in the plan.

**How to apply:** a new way for a lifetime to end needs a recorded list on its ast (copied by `Clone`) and an emit at
the matching codegen point; analysis that changes what is owned must keep `memory_state` honest, as every list is read
from it. Probe a change by grepping the probe's `.ll` for `drop` calls at the expected `!dbg` lines. See
[[drop-needs-forced-instantiation]], [[variant-drop-is-a-no-op]], [[spp-same-scope-shadowing]].
