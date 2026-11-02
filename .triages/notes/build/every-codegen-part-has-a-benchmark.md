---
name: every-codegen-part-has-a-benchmark
description: "A new AST with Stage11_CodeGen, or a new codegen module, needs a run-time benchmark covering it or a reason in benchmarks/exempt.txt; pre-commit fails otherwise"
updated: 2026-11-02
---

`benchmarks/coverage.py` (the `benchmark-coverage` pre-commit hook, so CI's lint job too) lists every
`sources/spp/asts/*_ast.cpp` that defines `Stage11_CodeGen` (as `ast:<stem without _ast>`, so `type_ast.cpp` is
`ast:type`) and every `sources/spp/codegen/*.cpp` (as `codegen:<stem>`). Each must be covered by a run-time benchmark: a
folder `benchmarks/runtime/ast/<name>`, or a `covers` entry in any `benchmarks/runtime/*/*/manifest.toml`. Otherwise it
needs a line in `benchmarks/exempt.txt` with its reason. It also fails on a stale exemption (the part is covered) and on
a name that matches no part.

**Why:** the run-time benchmarks exist to show every area of codegen getting slower or faster; an AST added without one
is an area nobody would notice regressing.

**How to apply:** adding an AST with codegen or a codegen module, either add it to the `covers` list of the benchmark
whose hot loop already leans on it, or write a benchmark (`main.spp`, `main.c`, `main.rs` doing the same work and
printing the same output, see `benchmarks/runtime.py`'s docstring and [[spp-test-writing-idioms]]), or exempt it if
it emits no code that runs. Renaming or deleting an AST file breaks the check until the manifest or exemption follows.
