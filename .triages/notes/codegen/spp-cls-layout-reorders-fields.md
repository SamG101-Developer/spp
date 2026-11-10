---
name: spp-cls-layout-reorders-fields
description: "a cls is reordered by descending alignment/size unless it is !ffi or !conv(\"C\"); a C struct needs one of them (since 2026-11-10)"
updated: 2026-11-10
---

The default ("spp") convention reorders a `cls`'s fields by descending alignment, then descending size, to minimise
padding (`SortMembersForSppLayout`, `sources/spp/codegen/llvm_layout.cpp`). `FieldIndexMap` keeps s++ field access
correct, so this is invisible from s++, and wrong across the FFI boundary, where C writes the fields in C's order.
**Since 2026-11-10 an `!ffi` or `!conv("C")` class keeps the order written** ([[abi-conventions-table]]); tuples always
did. `tests/codegen/check-ffi-layout.sh` checks both orders in the IR.

Seen 2026-09-09 on `sppc::CStat`: the declared order was wrong *and* the struct was 128 bytes against the real
`struct stat`'s 144, so every `fstat` overran the caller's frame and the stack canary aborted. Reordering the
declaration fixed the size but not the values (`%"sppc::CStat" = type { CTimespec, CTimespec, CTimespec, i64 x10, i32
x4 }`, the timespecs hoisted to the front). sppc's structs have since been declared largest-aligned first, so the sort
leaves them alone; that is luck, not a guarantee, until the stub's classes are `!ffi`.

**Why:** a C struct has one layout, the one its header gives; the s++ sort silently moves fields.
**How to apply:** any class that crosses to C (a stub record, a hand-written binding) is `!ffi`; the stub generator
(`../sppc/tools/main.py`) emits it on every record. Still open: checking a struct's layout against its C header at
build time (`../../../.triages/safety-hardening-plan.md`).

Related: [[sppc-sibling-repo]], [[stack-protector-heuristics]], [[ffi-struct-params-c-abi]].
