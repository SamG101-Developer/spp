---
name: err-args-hold-references
description: "ERR_ARGS keeps references to its arguments; an AST made inline in it (WithoutConvention, a fresh InferType) dangles and crashes"
---

`ERR_ARGS(...)` is a lambda returning `MakeErrArgs(args...)`, a tuple of **references**, built before the error is.
Any argument that is the only owner of a fresh node dies when that lambda returns: `*x->WithoutConvention()`,
`*ref.AstIn(scope)`, and `*x->InferType(sm, meta)` since phase 1 (the default `InferType` builds a fresh copy). The
error then reads freed memory: a test crashed with "killed by signal" and no message (2026-10-08,
`test_invalid_static_method_call_with_an_owned_receiver_of_another_class`).

**How to apply:** build such an AST into a local first, on the error path only (`if (bad) { const auto t = ...;
Raise<E>(..., ERR_ARGS(..., *t)); }`), or pass a resolved type as `ErrTypeAt(ref, node)`, which is held by value.
A node owned elsewhere (a symbol's `FqName()`, a written `Type`) is safe inline.
