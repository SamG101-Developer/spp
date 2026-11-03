---
name: borrow-not-a-generic-argument
description: A written borrow as a generic argument is E2, except under a generator or function type (found via sups); substituted types are exempt
updated: 2026-11-03
---

A borrow written as a generic argument is E2 (`Vec[&Str]`, `Vec[Vec[&Str]]`, `id[&Str](&a)`). The exceptions are the
arguments of a type that is, or superimposes, `Gen`/`GenOnce` or `FunRef`/`FunMut`/`FunMov`
(`marker_sups::HoldsNoValueOfItsArgs`), and everything inside them (the `(&T, &T)` in `FunRef[(&T, &T), Bool]`).
`Iterator`, `Indexed` and a user's `cls X ext GenOnce[T]` qualify through their sups. The exemption is the meta flag
`AllowBorrowedTypeArgs`, which is set while an exempt type's arguments are named *and* analysed.

**Why:** `Vec` stores `T` in raw memory, so no field check caught `Vec[&Str]`. A local `Vec[&Str]` outlived its target
(pack-design.md P0). Types with a field of `T` (`Opt`, `Tup`, `Box`) were already E2 at the field.

**How to apply:**
- The check lives in `TypeIdentifierAst::Stage7` (`check_borrowed_args`), and an explicit function argument is checked
  in the call AST. It only runs from stage 6, once sups are attached, and is re-checked on the `_HasAnalysed` path.
- Only a type the parser built (`_IsSourceWritten`) is judged. Types the compiler builds (a variant's
  `Var[Variants=Tup[&A, B]]`, a variadic's `Tup[Ts]`, a closure's `FunRef`) hold borrows legitimately: `&StrView or U64`
  as a parameter is a supported variant.
- An argument counts as borrowed by `type_predicates::IsTypeBorrowed` (deep), so a variant holding one counts too
  (`Vec[&Str or None]`). A borrow inside an exempt argument (`Vec[FunRef[(&Str,), Bool]]`) does not.
- `generic_inference::NamedGnArgs` analyses arguments while naming them. Any new step that analyses an exempt type's
  arguments must run with the flag set too, or a closure's `FunRef[Tup[&X], R]` trips on its `Tup`.
- Substituted types were not written, so the instance makers (`FindOrMakeClsInstance`, `MakeSupInstance`,
  `FindOrMakeFnInstance`) and `AnalyseSubstitutedType`/`InstantiateForScope` set the flag. `T = &StrView` from
  `Iterator[&StrView]` gives `nth` an `Opt[&StrView]`, and that must not be judged as written.
- `Opt[&Str]` is caught earlier, by `Some`'s field check in stage 5, and is still reported at the written `&Str`
  ([[instantiated-from-error-site]]).

Related: [[borrow-is-param-convention]].
