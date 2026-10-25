---
name: spp-dollar-mock-types
description: "\\\"$\\\" mock types: TypeEq falls back to TypeFuncEq for them, they must be registered globally, and IsTypeFunction says yes to them"
updated: 2026-09-12
---

A `$`-prefixed type (`TypeIdentifierAst::IsCompilerGeneratedType()`, i.e. `Name[0] == '$'`) is a nominal type standing
in for a callable: one per named function since stage 1, and - as of the thread-safety work - one per closure
expression. Three things about them are not obvious:

1. `TypeEq` already falls back to `TypeFuncEq` when either side is a `$` type (`type_compare.cpp`, the
   `stripped_lhs_sym->Type != stripped_rhs_sym->Type` branch), matching the mock against any function type it
   superimposes. This is why giving closures their own type does **not** break `let f: FunRef[(), S32] = ...` or a
   direct `f: FunRef[(), S32]` parameter.
2. Because of (1), `IsTypeFunction` answers **true** for a mock. `FindFnSup` therefore has to skip the type itself
   as a candidate for `$` types, or a call built from it looks for the `Args`/`Out` generic arguments only the real
   `FunXxx` carries - `CreateCallablePrototype` segfaults on the null `TypeAt("Out")`.
3. A closure mock must be registered in `sm->GlobalScope`, not the module scope. A closure written in one module and
   passed to a generic in another (`std::iterator::filter`) is resolved again inside *that* module's scope, and a
   module-scoped registration gives `SppIdentifierUnknownError` on the `$closure...` name.

4. A named function used as a value (`spawn(f)`, `call_it(f)`) is the per-function `cmp f: $F = $F()` constant from
   `FunctionPrototypeAst::Stage1_PreProcess`. Stage 9 skips `$` types, so until 2026-09-11 Stage 10 emitted it as
   `{null, null}` and any call through it jumped to address 0 (seen as a segfault in `closure.fn..` on a spawned
   thread). Fixed 2026-09-12 in the slim form (a ~680-line first version was rejected as too big):
   - **A call through a mock-typed value is a direct call:** `GetFnOwnerTypeAndFnName` (overload_utils) swaps in
     the function's name + owner scope via `fn_values::GetFnValueName`, so ordinary overload resolution does
     overloads/generics. Covers generic params (`spawn(f)`) and untyped `let f = overloaded`. The `cmp` constant stays
     `{null, null}` and nothing reads it (except an intrinsic/FFI taking the raw pair, e.g. `CClosure::from(named)`).
   - **Named-function mocks are `Copy`** (`ClassPrototypeAst::Stage5`), since a direct call no longer consumes the
     callee the way a closure call's `_ClosureDummyArg` does.
   - **Concrete `FunXxx` slots** (param, typed `let`, `ret`, assignment, attribute) need a real pair: Stage 7 calls
     `fn_values::InstantiateFnValue` after its `TypeEq`; codegen calls `codegen::CoerceToFnValue` next to
     `CoerceToVariant`, which re-chooses via `FindFnValue` (`FindRequiredFnInstance`) and builds
     `{<mangled>.as_value thunk, null}` (`BuildFnValue`; the thunk drops the env arg).
   - `fn_values::MatchFnValue` picks the overload from the mock's `DirectSupScopes` (each `sup $F ext FunXxx {
     fun }` block): kind compared bare through the sup chain, `Args`/`Out` in the block's scope (where a generic `T`
     resolves - comparing in `FunRef`'s class scope cannot see it). `TypeFuncEq` falls back to it after its old loop.
   - **Trap:** the name is NOT findable by scanning `ScopeDefinedIn->GetAllVarSymbols(true)` for the `cmp` alias - that
     returned 0 symbols. Read `proto->Name` off the block, owner = block's `Parent`.
   Tests: `regression::tst::codegen::test_a_named_function_passed_by_value_is_callable`,
   `..._an_overloaded_function_passed_by_value_is_the_overload_asked_for`,
   `..._a_generic_function_passed_by_value_is_instantiated_for_its_target`; std `test_a_named_function_can_be_the_thread_body`.

**Why:** (2) and (3) each cost a full debug cycle - (3) an unknown-identifier error deep in std, (2) a segfault.

**How to apply:** when adding any new synthetic nominal type, copy the dummy-scope pattern in
`GenericParameterTypeAst::Stage2_GenTopLvlScopes` (bare `ClassPrototypeAst` + `Scope` + `TypeSymbol`, ownership parked
in a static cleared from `ScopeManager::Cleanup`), and remember `IsCopyable` only walks sup scopes for a type that has
generic arguments - see [[zero-type-implies-copy]].
