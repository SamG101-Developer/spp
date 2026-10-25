---
name: spp-ir-diff-verification
description: "Verify spp refactors by diffing generated .ll files, but normalise Uid addresses first"
updated: 2026-09-16
---

To prove a refactor is behaviourally transparent, diff `project/out/x86_64-pc-linux-gnu/dev/llvm/**/*.ll` before and
after (excluding `lto.ll`, see below; compare the binary `spp.o` by its sorted `nm` names instead).
It is a much stronger check than comparing program stdout, and cheaper than the gtest suite
(which [[build-workflow]] says not to run).

**Three traps that made a whole session's "IR identical" claims vacuous (2026-09-15):** (1) glob recursively
(`find -name '*.ll'`) - the top level of `llvm/` holds only `main.ll` and `lto.ll`, every module is in a subdirectory,
so `for f in base/*.ll` compared one file (none, once `main.ll` was excluded). (2) A build from a *copied* project dir
differs in every module's `ModuleID`/`source_filename`; normalise the whole absolute prefix, not just the dir name.
(3) Print the compared count and treat 0 as failure - a missing dir or empty glob reports "0 differing". (4) Check the build's exit status first: a
failed `spp build` leaves the previous run's `.ll` files in `out/`, so the diff reports "0 differing" against stale IR
(2026-10-08, a std E65 hid behind it until the probe also failed).

**The scratchpad's `alpha_eq.py` does not normalise paths** (2026-09-16), only `$ADDR_` and quoted names - so trap (2)
above bites every time it is pointed at a *different* directory, and it reports "N of N differ; 0 names renamed" with
`ModuleID`/`source_filename` as the only differing lines. `alpha_eq2.py` beside it adds the root-prefix replacement;
prefer it for any cross-directory comparison. A same-vintage snapshot is the only meaningful base: compare a re-run of
`pr_s1` against `pr_base` (or `pj_t` against `pj_c`), not against an `irbase7_*` baseline, which was copied out of a
different project dir and from *unpatched* std. Live dirs: probe `pr_s1`/`pr_base` (93 `.ll`), project `pj_t`/`pj_c`
(196 compared), output under `out/x86_64-pc-linux-gnu/dev/llvm`. `probeE` is stale - `spp build` re-fetches std from
SPP-STL, so its copy lacks the local `drop(...)` patch and fails E94 in `option.spp`; that error there means the wrong
directory was used, not a regression.

**When a change renames things on purpose** (mangling), check equality *up to renaming*: per file, replace every quoted
llvm name - `%"…"`, `@"…"`, and block labels, which are *defined bare* as `"….exit":` at line start and referenced as
`%"…"` - by its index in order of first appearance, then compare. A one-to-one rename passes; two types merged or split
fails. Missing the bare label form makes every inlined-function label look like a real difference (2026-09-15).

**`spp::utils::Uid(ast)` embeds `reinterpret_cast<uintptr_t>(ast)`**, so IR symbol names are never
byte-reproducible across runs — ASLR alone changes them. Normalise before diffing:

```
find <dir> -name '*.ll' -exec sed -i 's/\$[0-9]\{6,\}_/$ADDR_/g' {} +
```

The trailing `_<counter>` suffix IS deterministic, so after normalising, identical files prove the
same number and order of AST/Uid allocations.

**NOT cosmetic noise** (corrected 2026-08-31): the trailing garbage bytes after `target triple`
(`"x86_64-pc-linux-gnu\x11\xdb\x02..."`) are the *visible* end of live memory corruption in the LTO
module, not a harmless artefact. `project/out/llvm/lto.ll` is `file`-typed `data`, never `ASCII text`;
per-module `main.ll` is clean, so the corruption is introduced at/after `LinkIntoLtoModule`.

It becomes **fatal** when it lands on an intrinsic declaration name instead of the triple: the suffix
of an overloaded intrinsic gets overwritten with `Uid()`-shaped fragments
(`llvm.usub.with.overflow0_11383`, `llvm.lifetime.endDB\xdb`), differing every run. An unsuffixed or
garbled `llvm.*` name is emitted as an ordinary external symbol, so it fails at *link* time with
`undefined reference to llvm.usub.with.overflow`. This is what `RepairMisnamedIntrinsics`
(`llvm_passes.cpp` (split 2026-10-19)) exists to paper over, and its detector misses the case where the mangled name still
resolves to a valid intrinsic ID.

`spp build`/`spp run` survive it; **`spp test -l std` does not** — more intrinsics are instantiated, so
corruption is likelier to hit one. Chase with [[asan-build-for-llvm-crashes]].

## Measured 2026-08-31 — what it actually is

Instrumenting right after `Intrinsic::getOrInsertDeclaration` shows the name is corrupt **at creation**,
in a fixed shape: correct base, `\0`, junk, correct overload suffix, with the base occupying **exactly
31 bytes** every time regardless of the real length. The junk is real bytes (hex-dumped), not a bad
length alone — neighbouring heap: S++ source fragments (`_u32, ]`), identifiers (`Error`), pointers.

Ruled out, each by direct test:
- **Not the `import llvm;` module boundary.** Routing the call through a plain-header TU
  (`llvm_passes.cpp` (split 2026-10-19)) gives byte-identical corruption. Note: adding llvm headers to a module TU's GMF
  makes gcc ICE with *"canonical types differ for identical types"*, so that mixture is not an option.
- **Not version skew.** Headers, static libs and the `llvm.gcm` BMI are all 23.1.0 and mutually newer.

Two traps for anyone resuming this:
1. **By the time `RepairMisnamedIntrinsics` runs, the suffix is already gone** — every declaration it
   sees has `size == 31` exactly. The object writer then truncates at the `\0`, which is why the linker
   reports the bare `llvm.usub.with.overflow`. Nothing can be "rejoined" at that point.
2. **The repair must stay after `verifyModule`.** LLVM's verifier computes expected names through the
   same broken lookup, so it *rejects the correct name* (`Intrinsic name not mangled correctly for type
   arguments!`). Repairing at the creation site fails for this reason.

## RESOLVED 2026-08-31 — without rebuilding LLVM

Two changes, both in `llvm_passes.cpp` (split 2026-10-19), took `spp test -l std` from "cannot link" to 162/180 passing.

**1. `RepairMisnamedIntrinsics` rebuilds the name properly.** Base name = the part before the field's
terminator (or the longest resolving prefix when junk ate the terminator); suffix = recomputed from the
signature via `isSignatureValid` + `getName`, taken from past offset 31. Three traps in doing this:
- **Never hand LLVM a `std::string`.** Contents are fine, `.size()` is not, so `setName(std::string)`
  puts the junk straight back. Measure with `strlen` on our side and pass `StringRef(p, n)`.
- **Detector must also collect declarations whose ID resolves but is overloaded** — a bare base name
  reports a valid ID and reads as healthy, yet only `…overflow.i64` is a name LLVM lowers.
- Guard against re-appending a suffix a name already has (`…overflow.i64.i64`) and against
  `fixed == fn` self-erase. Drop `llvm.lifetime.*` hints from the *base* name, before suffix rebuilding.

**2. Disable `CodeGenPrepare`** (`cl::getRegisteredOptions()["disable-cgp"]`, set in
`HostTargetMachine`). This was the last undefined symbol and the hardest to see: CGP synthesises
`llvm.usub.with.overflow` out of ordinary subtract-and-compare **during lowering**, after every pass we
control, so the misnamed declaration it creates cannot be repaired and cannot be avoided in `.spp`
source — any bounds check compiles to that pattern. Costs codegen quality, not correctness.

## It is NOT an ABI or configuration problem — don't chase that again

Checked and ruled out, all cheap:
- `libLLVMCore.a` has 255 `__cxx11` symbols and zero `_ZNSt3__1` (libc++), so LLVM is libstdc++ with
  `_GLIBCXX_USE_CXX11_ABI=1` — **the same ABI spp builds with**. No mismatch.
- `LLVM_DIR=/usr/lib/llvm-23/cmake`, headers `-isystem /usr/lib/llvm-23/include`, same install, 23.1.0.
  No headers-from-one/libs-from-another skew.
- Building spp with clang is a dead end regardless: the other libs are gcc-compiled and the module BMIs
  are gcc `-fmodules-ts` artefacts. (And clang on linux uses libstdc++ anyway, so it would not have
  changed the layout.)

So it is a defect in this LLVM 23.1.0 build itself — most likely how its intrinsic name table was
generated. Rebuilding LLVM with different flags is not indicated. **The workaround above is the answer,
not a stopgap.**
