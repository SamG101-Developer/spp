---
name: spp-cls-layout-reorders-fields
description: every cls is reordered by descending alignment/size, so no cls can describe a C struct - the FFI stubs are all suspect
---

`codegen::SortMembersForSppLayout` (`sources/spp/codegen/llvm_layout.cpp:19`) reorders the fields of **every** `cls`
by descending alignment, then descending size, to minimise padding. There is no opt-out and no `repr(C)`-style
annotation. `FieldIndexMap` keeps s++ field access correct, so this is invisible from s++ — and completely wrong
across the FFI boundary, where C writes the fields in C's order.

Seen 2026-09-09 on `sppc::CStat`. The declared order was already wrong *and* the struct was 128 bytes against the
real `struct stat`'s 144, so every `fstat` overran the caller's frame by 16 bytes and the stack canary aborted the
process with "stack smashing detected". Three lines of s++ reproduce it, with no async or threading involved:

```
let path = std::fs::path::Path::new("f.txt")
let file = std::fs::file::File::open(&path, std::fs::file::FileMode().read()).expect("")
let size = file.file_size()      # <- aborts here
```

Correcting the declared order and padding fixed the **overflow** (the total size is now right), but not the
**values**: the IR shows `%"sppc::CStat" = type { CTimespec, CTimespec, CTimespec, i64 x10, i32 x4 }` — the three
16-byte timespecs hoisted to the front — so `size` still lands on `atim.tv_nsec` and reads a nanosecond count
(227580418 for a 20-byte file).

**So: fixing an FFI struct by reordering its declaration does not work.** The real fixes are either a `!c_layout`
annotation that skips the sort, or narrow accessors on the c side that return one scalar through an out-parameter —
which is the style the rest of `sppc` already uses (`sppc_pthread_equal(h1, h2, uint64_t *out)`), and why only the
struct-passing calls are affected. Every multi-field `cls` in `ffi/sppc/stub.spp` is suspect until audited;
`CTimespec` (two `U64`) is safe only by luck.

The safety doc already lists this as open: "Struct layout verification against the C header at build time" in
`../../../.triages/safety-hardening-plan.md`.

Related: [[sppc-sibling-repo]], [[stack-protector-heuristics]], [[std-known-gaps]].
