---
name: spp-linear-types
description: "S++ ownership is linear as of 2026-08-26 - every non-Copy value used exactly once, no implicit drops"
updated: 2026-08-26
---

S++ moved from affine to **linear** ownership on 2026-08-26. Every non-`Copy` value must be used exactly once.
Destruction is no longer compiler-inserted; it is an ordinary consuming method, which is what makes it fallible -
`File::close(self) -> Res[(), IoError]` can report that closing failed, where the old `Del::del` could not.

The five rules, all in Stage8 except the last:

1. **Scope exit** - a symbol still live at the end of a function-body scope is an error. Live = initialized and not
   fully partial-moved. Exempt: `Copy` types, borrows, `cmp` symbols, generics. "Fully partial-moved" (every
   non-copyable part taken, e.g. `ret self.val` on a one-field class) counts as consumed **on purpose**. The maintainer confirmed
   it 2026-09-28, and case patterns rely on it. `TestMemSecondClassBorrows` pins it, alongside the E94/E106 shapes.
2. **Parameters are locals** - `mov` params and `mov self` obey rule 1 uniformly; `&`/`&mut` are exempt.
3. **Early exits** - `ret` and loop `exit`/`skip` leave several scopes at once, so their closing braces are never
   reached; `CheckLiveUpToFn` / `CheckLiveUpToLoop` check those scopes at the jump instead.
4. **Destructure consumes** - `let Self(fd) = self` leaves `self` *moved*, not partial-moved. This is the terminator,
   and it has to exist: a struct whose attributes are all `Copy` has no attribute that can be moved off it, so partial
   moves alone could never account for it. Single-attribute access (`let fd = self.fd`) is unchanged.
5. **No discarded values** - a statement's value must be `Void` or `Never`. Discard propagates inwards, so a `case`
   written as a statement is reported against each branch's final expression, not against the `case`.

Code: `analyse/utils/linear_utils.{ixx,cpp}` (`IsLive`, `CheckScopeExit`, `CheckLiveUpToFn`,
`CheckLiveUpToLoop`), `destructure_utils::ConsumeDestructureSource`, `expr_utils::ValidateDiscardedValue`,
`InnerScopeExpressionAst::DiscardsFinalMember` (true only for a function body, since S++ returns through `ret`).
Errors E94 `SppLinearValueNotConsumedError`, E95 `SppDiscardedValueError`, E96
`SppLinearValueSkippedInDestructureError`. See [[no-destructor-dispatch-yet]] for what was deleted.

**No implicit consumption, anywhere (maintainer, 2026-10-09).** Two rules decided for the memory refactor
(`.triages/docs_closed/memory-analysis-refactor.md` M5), both errors, for the same reason: the point of linear typing is that a resource
is never dropped by accident, and an implicit drop on overwrite or at the end of an expression is exactly that ("we
might as well make all dropping optional"):
- a non-`Copy` **temporary** must be consumed by the full expression that makes it - borrowing, reading a field,
  comparing or matching on it is an error (`mk().a`, `Str::from("a") == Str::from("b")`; open-bugs #1);
- **overwriting a live linear value** (`x = f()` while `x` still owns one) is an error - move or drop it first
  (`CheckOverwrite`; ~43 tests that do `let mut x = f(1); x = 2` get updated; open-bugs #2).

**Enforced since 2026-10-10 (memory refactor M5).** Temporaries: `linear_utils::IsLinearTemporary` + three checks
(borrowed temporary, attribute read leaving owned parts, `case` over one keeping it); a `cmp` value is exempt. Overwrite:
`CheckOverwrite` / `CheckOverwritePart`. **Writing through a borrow** (`self.a = ..` with `&mut self`, `x@ = ..`) of a
non-`Copy` value is always an error (the maintainer): it cannot be moved out first, so the write would drop it - exchange with
`std::mem::ops::replace` and consume the old value. `std::mem::ops::swap` is an intrinsic for the same reason (no
re-borrow exists: `&mut a@` needs `Copy`). Exempt: abstract `Self` in a class's default method (`Clone::clone_from`).

**Generic discard needs a `Drop` bound.** Found the hard way: `unwrap_or(self, default: T)` must discard `default` on
one branch and the payload on the other, and `filter` must discard the value when the predicate fails - a generic `T`
has no consuming method to call, so these are unimplementable without one. The decision was a `[T: Drop]` constraint
plus an explicit `val.drop()` call, *not* a compiler-exempt `mem::forget`. So `Del` survives renamed to `Drop`, as an
explicit trait rather than a compiler-inserted one.

**`case` takes its subject per edge (maintainer, 2026-10-10; memory refactor M4.3).** A branch takes the subject on its match
edge if a pattern binds by move; a branch narrowing it to members that are all `Copy` (`is None`) owes nothing
(`MemoryState::OwesNothing`); an `else`, or falling through with no `else`, keeps it owned and it must be consumed there
(`else { drop(x) }`). The fall-through carries the complement type; an irrefutable pattern or full variant coverage
means none. Short-form `case x is P` and `x is P` behave the same (fixed open-bugs #3: later use is E8). Planning is
`case_utils::PlanSubjectEdges`; the edge is applied by `case_utils::ApplySubjectOnMatch`. std had real leaks of the
other `Res` variant on `else` paths; fixed by `drop(self)` there. See [[spp-same-scope-shadowing]] for why an `is`
declares its bindings at its position.

**The stdlib is a separate repo** - `SamG101-Developer/SPP-STL`, cloned into `project/vcs/std`, untracked here, and
`spp build` re-fetches it each run. Its migration cannot land in a commit of the compiler repo.

**Closure parameters are held to the rule (fixed 2026-09-10).** They used to be exempt by accident: parameters and
captures share the `closure-outer` scope, closures never ran `CheckScopeExit`, and a `ret` stopped at `closure-inner`
because stage 8 set `EnclosingFnScope` after moving into it. Now `ClosureExpressionAst::Stage8` sets it to
`closure-outer` (matching stage 7) and checks that scope at the end of a body that does not terminate, like a function.
Captures are exempt through `VariableSymbol::IsCapture` (set in `ClosureExpressionCaptureGroupAst::Stage7`, read in
`IsLive`): the environment owns them and re-reads them per call, and the closure value is what is held to being
consumed. Fallout was real std bugs, not false positives: four `map_err((e: NullptrErr) AllocOomErr)` in
`allocator.spp` and one `(e: Utf8Err)` in `io.spp` ignored their error. The maintainer chose to make **`NullptrErr` and `Utf8Err`
`Copy`** (`sup X ext Copy { }`) rather than add `drop(e)` at each site - Rust's `Utf8Error`/`AllocError` are `Copy`
too - so the closures stayed one-liners. `!zero_type` alone does not make a marker `Copy` (see
[[zero-type-implies-copy]]); the other 31 zero-type error markers in std are still linear.
