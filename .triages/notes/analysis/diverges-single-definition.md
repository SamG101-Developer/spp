---
name: diverges-single-definition
description: "StatementAst::Diverges(sm, meta) virtual is the one definition of \\\"never falls through\\\" (Terminates() and expr_utils::Diverges removed 2026-10-23); no dead code at all (E28), even after `loop true { }`"
updated: 2026-10-23
---

Since 2026-10-23 (N4.1), `StatementAst::Diverges(sm, meta)` is the only answer to "does control continue past this?".
It is a virtual method. It replaced two things that disagreed about `case`: the free
`expr_utils::Diverges` and the `Terminates()` virtual. The free function had been the single answer since 2026-09-27.

The overrides:
- Base `StatementAst`: false.
- `ret`, `exit` and `skip`: true.
- `ExpressionAst`: its type is `!`, read with `IgnoreMissingElseBranchForInference`.
- `let`: asks its value's `Diverges`.
- A block: its last member, read in the block's own scope.
- `case`: nothing falls through (the plan's `Coverage.FallsThrough`) and every reachable branch diverges. Before
  stage 7 has made the plan, it falls back to "an `else` is last, and every branch diverges".

**Every caller routes through it:**
- E28 unreachable code, per statement in `InnerScopeExpressionAst::Stage7`
- the `case`'s `!` type
- case branch types and memory merging (`case_utils`)
- the branch codegen's `body_is_never`
- the loop second pass
- the scope, function and closure end `CheckScopeExit` calls
- missing `ret` (E58, in `subroutine_prototype_ast.cpp`)
- `defer` terminating

**the maintainer's rule: uniform behaviour, so NO dead code.** Code after an exit-less `loop true { }` is E28 too. Tests that
used `loop true` as a placeholder were changed to `loop b` with a `b: Bool` parameter, which keeps a second iteration
and a reachable follow-on. `exit` was added only where the test was about a single pass.

**Gotcha:** when every branch of a `case` diverges, its type check must use only the `!`-typed branches. The
old fallback to "all branches" compared a `ret` branch's `Void` against the target (E1 in std `Result::zip`).

**How to apply:** a new check that asks "does this finish" calls `Diverges`. A new statement kind that always jumps
overrides it. Related: [[spp-linear-types]].
