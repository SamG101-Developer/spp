---
name: comp-args-target-their-param-type
description: A comp argument is analysed against its parameter's type (GenericArgumentAst::ExpectedCompType), set as arguments are named
updated: 2026-10-26
---

A comp argument's value is analysed as an expression of its own (no enclosing `let`/`ret` target applies), **against
its parameter's type**, which is carried on the argument as `GenericArgumentAst::ExpectedCompType`. One shared method
does that analysis, `GenericArgumentAst::AnalyseCompExpr`: `AnalyseCompVal` and `NameCompArgs`' `bind` both call it.
Without it, an explicit array of variants (`[Some(val=1), None]` for `cmp n: [Opt[U64]; 2_uz]`) held every element to
the first one's type, giving E1. That has been true since `6fc4d47f`, which cleared the target.

Where it is set:
- `generic_inference::NamedGnArgs(..., params_scope, ...)` calls `comp_generics::ExpectCompParamTypes` on its own copy
  before naming, so a positional argument carries the type into `bind`. All four callers pass the scope their
  parameters are declared in: a type name, an object initializer, each overload candidate, and an alias.
- A call analyses its generic arguments before resolving an overload, so it sets them from
  `OverloadSolver::ExpectedCompArgTypes`, the comp twin of `ExpectedArgTypes`. That is set only when every candidate
  agrees.

Arguments are paired with parameters by `type_resolution::ParamsBoundByArgs`, everywhere. The call's evaluator uses it
too, so a positional comp argument binds its parameter by name even when later parameters are defaulted.

**Why:** a parameter's type is its argument's target, as a `let` type is its value's.

**How to apply:** no type is given for `cmp n: T` (not concrete until `T` is bound), nor for a variadic parameter. A
new place that analyses a comp value goes through `AnalyseCompExpr`, and a new caller of `NamedGnArgs` passes the
parameters' scope. Tests: `TestGenericArgumentComp.test_*variant_array*` and the two `test_invalid_array_*` controls.
