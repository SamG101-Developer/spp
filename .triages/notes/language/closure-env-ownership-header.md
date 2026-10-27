---
name: closure-env-ownership-header
description: "A closure's environment header is {i8 owns, ptr drop_captures} (N5.5a); allocate/index/free/drop only via the llvm_fn helpers; sppc reads the same header"
updated: 2026-10-24
---

A closure's environment is `{i8 owns, ptr drop_captures, captures...}` (N1.5 added `owns`, N5.5a on 2026-10-24 added
`drop_captures`). `owns` is 1 for a heap environment (any by-value or `&mut` capture), 0 for a stack one (only `&`
captures); a capture-less closure has a null env. `drop_captures` is `<closure>.drop_captures(env)`, built by
`ClosureExpressionAst::_BuildDropCaptures` when a by-value capture needs dropping (null otherwise), KCFI-stamped with
`StampCfiClosureEnvDrop`. All of it lives in `spp.codegen.llvm_fn`: `ClosureEnvType`, `EmitNewClosureEnv(env_ty,
on_heap, drop_captures, ...)`, `ClosureEnvCaptureIndex(i)` (= i + 2), `EmitReleaseClosureEnv` (frees only non-null and
owned: a consumed `FunMov` call, whose body consumed the captures) and `EmitDropClosureEnv` (calls `drop_captures`,
then releases: `FunMov::drop`).

**sppc:** `headers/sppc/closure.h` mirrors the header as `sppc_closure_env`, and `_sppc_closure_discard` drops and
frees a closure the runtime gives up uncalled (failed `sppc_async`/`sppc_pthread_create`, `pthread_once` losers).
After a call the runtime still just `free`s the env. A closure built in C for the runtime must start its environment
with the header (see sppc's `tests/test_threads.c` once test), or pass a null env.

**Why:** freeing used to be decided from the callable's static type, and a `FunRef` (stack env) is accepted where a
`FunMov` is expected, so it was freed off the stack. Every closure of one signature shares a type, so neither ownership
nor the captures' destructor can come from the type; the header carries both.

**How to apply:** never GEP a capture at a raw index or call `sppc_malloc`/`sppc_free` for an environment directly;
use the helpers. A layout change must be made on both sides (compiler and sppc). Runtime behaviour is checked by
`tests/codegen/check-drops.sh` (gtests only compile). Related: [[closure-borrowed-capture-and-consuming-self]],
[[closure-captures-in-loops]], [[sppc-sibling-repo]], [[kcfi-bundles-not-metadata]].
