---
name: spp-symbol-mangling
description: "Function symbols are \"readable::path[Short=Generics]$h<fnv64>\" since 2026-09-22; closures are \"<enclosing>::{closure#N}$h...\"; the sppc backtrace strips the $h suffix"
---

`MangleFnName` returns the readable name (module path + name joined by `::`, owner generics in short form: last
segment only, `Tup` as `(a, b)`, `Var` as `a or b`, closure types as `{closure}`) plus `$h` + 16-hex FNV-1a of the
OLD full mangling (`path#name#[owner generics]#ret#params...`). The hash is what keeps overloads unique - s++
overloads on return type, so the readable part alone is NOT unique. `MangleClosureName(enclosing, N)` nests
`::{closure#N}` inside the enclosing symbol's readable part and reuses its hash; N is found by probing
`getFunction` in the emission module, so it is deterministic codegen order.

Nothing parses mangled names (checked: no `#`-splitting outside llvm_mangle.cpp); `MangleTypeName` (LLVM struct
names) and `MangleCmpName` are unchanged. sppc's `_sppc_bt_put_symbol` drops a trailing `$h`+16 hex digits.
Related: [[sppc-sibling-repo]].
