---
name: affine-ownership
description: "Ownership is affine since 2026-11-23: a non-Copy value moves at most once and is dropped by the compiler where its lifetime ends; where each drop point is computed and emitted"
updated: 2026-11-27
---

S++ was linear from 2026-08-26 to 2026-11-23 (every non-`Copy` value consumed exactly once, no implicit drops, E94).
It is now **affine**: a value that moves (`TypeRef::IsMoveOnly`: owned, not `Copy`, not `!`) is used at most once, and
whatever is still owned where its lifetime ends is dropped there by the compiler. Plan and history:
`.triages/docs_open/affine-migration.md`.

**Analysis decides, codegen emits.** Stage 8 records, on the ast that ends a lifetime, what is still owned there
(`drop_utils::OwnedValue`: the name, where it is declared, the parts already moved out), and Stage 11 drops exactly
that list (`codegen::EmitOwnedDrops` for a scope's own owners, `EmitOuterDrops` for values of enclosing scopes found
outward, `EmitScopeExit` for exits leaving several scopes, innermost first). All three live in `codegen/llvm_drop`. The
edge lists sit in each ast's own anonymous `_Drops` struct (one field per edge), and its `Clone` copies the struct in
one line, as some clones are generated without being analysed again (`tests/compiler/test_msc_ast_clone_drops.cpp`).
`drop_utils::AddDrops` adds a join's to a list, newest first.

| Drop point | Recorded by | On |
|---|---|---|
| Scope end (block, function / closure parameters, a closure's by-value captures, branch bindings) | `drop_utils::EndRegion` → `OwnedIn` | `_Drops.End` (block, branch, closure, function prototype) |
| `ret` / `?` (a lowered `ret`), `exit` / `skip` | `OwnedUpToFn`, `OwnedUpToLoop` | `_OwnedAtExit` |
| Generator frame destroyed at a `gen`, or before its first resume | `OwnedAtSuspend` | `_OwnedAtSuspend`, `_OwnedAtStart` |
| A lowered `GenOnce`'s by-value `gen` (its return) | the same list, passed to `EmitReturn` | `_OwnedAtSuspend` |
| Paths meeting that disagree (`case` branches and fall-through, loop exits) | `drop_utils::JoinPaths` | a branch's `_Drops.End`, an `exit`'s `_Drops.AfterJump` (both via `AddDrops`), a `case`'s `_Drops.FallThrough`, a loop's `_Drops.BadCond` |
| A guard's moves, on the edges passing its branch over (no match, guard false); its `$` temporaries on the false edge | `case_utils::CarryGuardMoves` (`JoinPaths`), `TemporaryRegion::Held` | `_Drops.BadMatch`, `_Drops.BadGuard` |
| A logical operator's right operand vs its short circuit | `JoinPaths` | `BinaryExpressionAst::_Drops.ShortCircuit`, `_Drops.AfterRhs` |
| Overwrite (`x = v`, `p.a = v`, through `&mut`, `x@ = v`) | `OwnedOnPath`, `OwnedBeforeWrite` | `AssignmentStatementAst::_Drops.Overwrite` (per target) |
| Temporary borrowed, read a part off, or `case`d over without being taken | `IsTemporary` sets a flag | pending list, `codegen::PushTemporary` |
| A lowering's hidden `$` temporary (`$cmp`, `$spread`, `$fold`, `$_dst_`, `$is`) at the end of its region | `drop_utils::TemporaryRegion` | a block's `_Drops.StatementTemporaries` (per member), `_Drops.RhsTemporaries`, `_Drops.CondTemporaries`, `_Drops.BadCond`, a `case`'s paths |
| Discarded temporary (a statement's value) | `expr_utils::DiscardValue` | `_DropsDiscarded` |
| A call fold's hoisted temporaries no call took | `OwnedOnPath`, then marked moved | `_Drops.Fold` |

- **No drop flags.** A value held on only some of the paths that meet is dropped at the end of each path still holding
  it, before they meet; after the join it is held on none (`OwnedIn` skips `MaybeInit`). Where they disagree only about
  parts, each path drops just those parts it holds (the value with everything outside them counted moved,
  `drop_utils::PathsOutside`), and after the join those parts are gone (a maybe-moved part counts as moved at the end)
  while what the paths agree on is still held. Earlier than Rust's scope-end drop, static. A loop's head is such a join
  too: the entry, the body's end and each `skip` meet there (every iteration after the first begins from it), and each
  drops on its way in what another does not hold (`_Drops.Entry`, `_Drops.BackEdge`, a `skip`'s `_Drops.AfterJump`), so
  in the body it is held on none (E8 on a read; an overwrite drops nothing). An `exit` or `skip` aimed at an enclosing
  loop files itself in that loop's frame on each pass over the inner body, so each inner pass cuts the enclosing frames
  back to where it found them: only the last pass's entries stand.
- **Only enclosing scopes' owners join.** `drop_utils::OwnersAround` starts at the `case`'s or loop's parent: their
  own scope holds `is` bindings that exist only where they matched, and the flow also holds branch-local symbols.
- **Pending temporaries** live on `ctx->Pending->Held`: a block statement drops what it pushed at its end, a
  `ret` everything since `meta->TemporaryBase` (set per function, coroutine, closure), an `exit` / `skip` since
  `LlvmLoopInfo::TemporaryDepth`, and a loop condition what it pushed once its value is made (each evaluation). A
  `case` over a temporary pushes it only inside branches that keep it.
- **Hidden `$` temporaries** (`VariableKind::Temporary`) end with the region that declared them
  (`drop_utils::TemporaryRegion`, opened on a scope before the code is checked; it reads its stretch of
  `Scope::Declarations`, which logs every declaration, a loop's later passes included). The regions: a statement
  (except one that is itself the `let $x = ..` a lowering reads after it, `OwnLetName`), a logical operator's right
  operand, a loop's condition and body (`Held` where the condition ends the loop, `End` at the body's end; a `skip`
  uses `TemporaryRegion::Whole`), a guard (on into the body; `Held` on the false edge), and a `case` condition (each
  path out). In code generation, `codegen::EndTemporaryRegion` drops the unnamed pending temporaries and these
  together. Paths meet through one helper, `drop_utils::JoinPaths`.
- **A pattern guard may move** an outer value (2026-11-24): the move holds on every path past its branch, so the no-match
  edge (where the guard never ran) drops it, and later branches see it held on none (E8). E114 is left only for a guard
  moving the branch's own non-borrow bindings, which are still parts of the subject.
- **Views** (2026-11-24): a name for a part of another value, owning nothing (`regions::IsView`:
  `VariableKind::FlowNarrowing` with `VariableSymbol::ViewRoot`/`ViewSteps`). A subject a pattern narrows is a view of
  it held as that member (`o.<Some[T]>`, a member step: `type_members::VariantMemberStep`); an `is` binding is a view of
  the place it was read off (`o.<Some[T]>.val.a`). `regions::PlaceOf` resolves a place rooted at a view to the owner's
  place, so every check and mark (`mem_utils::ValidateSymbolMemory` marks the owner) sees through it with no forwarding:
  moving `y.a` partly moves `o` through its member, and `EmitDrop` hands paths under a member to that member, as it
  hands taken-apart paths (a destructure through a view is taken apart in the owner, `mem_utils::TakeApart`). Binding a
  view takes nothing (`LocalVariableSingleIdentifierAst::Stage8`); a view cannot be `mut` (E4: it is generated as a
  copy, so a write would change the copy). A path into a variant naming no member uses the variant whole.
  `NarrowsSymbol`, its move/loan chains and the narrowed symbol's copied state are gone.
- **`is` bindings are scoped where the match holds** (2026-11-24): an `is` declares nothing where it stands
  (`IsExpressionAst::Bound`). An `and` whose left operand binds something analyses its right operand in a scope of
  its own (`Scope::IsOperandScope`), declaring what the left operand bound (`expr_utils::BoundWhenTrue`), as does
  the body of a loop, a guard's branch and a boolean `case`'s first branch (`InnerScopeExpressionAst::SetBoundOnEntry`),
  in every walk; nothing else makes one (a comp argument's operator is analysed off the walk). A clone walked where the
  original stood and never analysed again (a receiver, `InjectReceiver`; forwarding) shares `_RhsScope`; one analysed
  again elsewhere (`HoistOnce`: a fold argument) does not share `_LogicalAnalysed`, so its Stage 7 makes its own scope
  where it stands, as its `is` operands make their own `case` (`ConvertIsExprToCase` lowers from copies, leaving the
  `is`'s operands as written, so a clone can be analysed again). Only a compile-time re-read is walked out of order
  (`_InRhsScope` with a manager standing at the scope). Nothing is
  retracted: no statement end, no `or` or `else` hiding (the old `ExpiresWithStatement`, `ExpiredIsBindings`,
  `EndStatement` are gone). A hidden temporary made in an operand scope lives in the condition around it
  (`Scope::TemporaryHome`, `LocalVariableSingleIdentifierAst::DeclareIn`). `case o is P { .. }` is not this: it is
  rewritten to `case o of { is P { .. } }`, whose bindings own as any branch's.
- **A logical operator's right operand is a path of its own:** a move in it (`flag and take(x)`) is joined with the
  short circuit, which drops what it still holds. A hidden temporary it declares (`flag and mk() is Some(..)`'s `$is`)
  is ended at the operand's end and left out of the join, as the short circuit never made it. A chain of one operator
  nests to the right (`BinaryExpressionAst::_NestChainRight`: `a and b and c` as `a and (b and c)`), so `c` reads
  what `b` binds while `b`'s value is held.
- **An `and` as a condition** (a loop's, a guard's, a boolean `case`'s: `BinaryExpressionAst::MarkCondition`, passed
  down a right-nested chain) keeps its right operand's temporaries held where it answers true, so the body reads
  their `is` bindings (`loop i < n and next() is Some(v) { v }`). It drops them where the right operand answers false
  (a `logical.rhs_false` block), so after it they are held exactly where it is true (`HeldWhenTrue`). The consumer ends
  them on the true edge (the iteration's end, the branch's end, an `exit`/`skip`/`ret`) and treats them as gone on the
  false one: the loop's condition end and `else` (`Held(.., except)`), the guard-false edge, a boolean `case`'s later
  branches and fall-through (`ValidateInconsistentMemory`'s `held_in_first_branch`).
- **A field read off a temporary** (`mk().a`) is moved out of it, the rest dropped as the statement ends; borrowed
  (`f(&mk().a)`, a `&self` receiver) or discarded, nothing is moved and the temporary is dropped whole
  (`ReadByBorrow`, `drop_utils::FieldReadOffTemporary`).
- **A statement `case` yields no value** (`ValueDiscarded`): codegen makes no phi and never joins its branch types.
- **A destructure takes the value apart** (choice A, 2026-11-24): its type's own `drop` never runs on it, and the
  owned parts the patterns skipped are dropped field by field by its owner (`MemoryState::TakenApart`, carried to codegen
  as `OwnedValue::TakenApart` paths). Binding every owned part moves it whole. So `let Self(..) = self` is how a
  `close`-style consumer disarms `drop` without leaking fields, and skipping an owned part of a `Drop` type is no E106.
  A field moved out by access (`let a = h.a`) is still E106. Paths that disagree about it are a "maybe", dropped on each
  path before they meet; it is E106 too (moved on one branch only), so every E106 check and every drop reads the parts
  gone as `PartsGone` (`PartialMoves` plus `MaybePartialMoves`), never `PartialMoves` alone.
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
