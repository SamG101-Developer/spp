---
name: ice-for-impossible-states
description: "Impossible compiler states call spp::Ice (spp.utils.ice), never a SemanticError, std::unreachable or abort; SPP_ASSERT is live in release (2026-10-17)"
updated: 2026-10-16
---

Since N8.1/N8.2 (2026-10-17) the compiler has one way to stop on its own bug: `spp::Ice(message)`,
`Ice(ast, message)` or `Ice(ast, scope, message)` from `spp.utils.ice`. It throws `spp::InternalCompilerError`
(a plain `std::exception`, not a `SemanticError`/`AbstractError`), rendered at the throw with the compiler
file:line:function from `std::source_location` and a `std::stacktrace` on GCC (`stdc++exp` linked).

**Why:** the old `SppInternalCompilerError` was a `SemanticError`, so the sink's per-member recovery, overload candidate
checking (`OverloadSolver::_Check`) and the index-only `catch` in `TypeStatementAst` could swallow it; raised with `{}`
scopes it printed nothing. `SPP_ASSERT` compiled to nothing in release, which is every test and std run.

**How to apply:**
- A state the compiler's own code says cannot happen: `Ice`. A mistake in the user's source: a `SemanticError`
  (or a syntax error in the parser). The machine's fault (file, target, linker): an ordinary error.
- No `IceIf` with an ast on purpose: an eager `*p` would dereference null. Write `if (c) { Ice(...); }`, or
  `SPP_ASSERT(c)` for a one-liner (it needs `import spp.utils.ice;` in the file).
- Code in a file-level anonymous namespace outside `spp` must write `spp::Ice`.
- No `std::unreachable()` after a switch: `default: Ice(std::format("unhandled X: {}", static_cast<int>(v)));`.
- Only where the check really tells the impossible state apart. N8.5 (2026-10-17): the formatter's "position past
  this file" ICE fired in 11 index tests, but a formatter can't tell another file's ast in range either, so the
  cause was error-builder formatter pairing (N8.7, now fixed: positions name their file); it became "no position". Probe std first, then the
  full suite, before trusting a new ICE; audit record in `../../../.triages/failsafe-audit.md`.
- `Ice(ast, ...)` quotes through `ast.GetAstScope()`, which only scope-owning asts have; pass the scope for an
  expression. Related: [[spp-test-suite-stage-scope]].
