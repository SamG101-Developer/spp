# Project notes

Paths are relative to this folder. Read a note before relying on its one-line summary.

## Conventions
- [Prefer proper fixes](conventions/prefer-proper-fixes.md) — root-cause fixes, not shape-specific band-aids
- [Add tests even if failing](conventions/add-tests-even-if-failing.md) — red tests for known bugs wanted, plus a Todo at the site
- [Code reduction means functions](conventions/code-reduction-means-functions.md) — remove dead/duplicate/single-use fns; not macros
- [Symmetry levels up](conventions/symmetry-levels-up.md) — make the worse of a pair match the better
- [Type/comp symmetry helpers](conventions/type-comp-symmetry-helpers.md) — comp generics mirror type generics; shared helpers
- [Switch impls off TypeAst](conventions/switch-impls-off-typeast.md) — analysis/codegen move to symbols/TypeRef; big rewrites OK
- [Naming abbreviations](conventions/spp-naming-abbreviations.md) — Gn/Fn/Cls..., Get/Find/FindOrMake/Read/Of verbs
- [Identity renames 2026-10-06](conventions/identity-renames-2026-10-06.md) — old→new names map (TypeKey, CompKey, Stamp*, ...)
- [Terse comments, none in config](conventions/terse-comments-in-config.md) — no comments in config; brief in scripts
- [LF line endings only](conventions/lf-line-endings.md) — CRLF breaks git hooks; check `git ls-files --eol`
- [Map/Set, never std::unordered](conventions/spp-map-set-not-std-unordered.md) — Map/Set default, StableMap/Set if addresses escape
- [types.ixx: aliases not wrappers](conventions/dead-code-sweep-skip-types.md) — Vec is a std::vector alias (N0.1); Opt, Path, Arr, Span, Func, FuncRef aliases (N0.2-3)
- [import std, not STL headers](conventions/import-std-not-stl-headers.md) — no STL includes in a GMF for std facilities
- [Spelled compares OK off hot path](conventions/spelled-compares-ok-off-hot-path.md) — one-place "self"/"T" checks stay; N2.1/N2.3 struck
- [[CHECKED] markers are the maintainer's](conventions/checked-markers-are-sams.md) — never add one; removing or folding a marked function is fine
- [Check newer than apt](conventions/check-newer-than-apt.md) — any apt/brew-installed tool: compare upstream latest, pin if newer
- [genex, not std::ranges](conventions/genex-not-std-ranges.md) — range algorithms via genex; add `import genex;`
- [Normalised platform macros](conventions/use-normalised-platform-macros.md) — `#if SPP_COMPILER_MSVC` from macros-platforms.hpp, never raw _MSC_VER

## Build, test, verify
- [Build workflow](build/build-workflow.md) — release build; probe loop; targeted suite for targeted changes; rebuild first, count failed/, wipe after kills
- [README test badges](build/readme-test-badges-after-suite.md) — after a non-regressing full suite: total, passed, %
- [IR diff verification](build/spp-ir-diff-verification.md) — diff out/llvm/**/*.ll, normalise Uids, print count
- [Mangled names may change](build/mangled-names-may-change.md) — refactors may rename; check defines stay one-to-one
- [Compiler probe harness](build/spp-probe-harness.md) — scratch fixture for one-file probes; pipes hide segfaults
- [gtest-parallel crash reporting](build/gtest-parallel-crash-reporting.md) — segfaults say "killed by signal"
- [Test suite stage scope](build/spp-test-suite-stage-scope.md) — suite runs codegen + verify; LLVM context per compile (N1.12); identities cleared per compile (N1.13)
- [Test harness std refresh](build/spp-test-harness-stale-std.md) — SppBootstrap.Fixture pulls once per run; hand runs without it compile a stale std
- [Codegen checks suite](build/codegen-checks-suite.md) — tests/codegen/run.sh, ~6 min with check-intrinsics (KNOWN list in intrinsics.py); new checks on lib.sh
- [Test group bugs](build/spp-test-group-filter-bugs.md) — novel !test group segfaults; --group filter misbehaves
- [s++ test writing idioms](build/spp-test-writing-idioms.md) — destructure-only patterns, literal-only unary minus
- [Stale module objects](build/cmake-stale-module-objects.md) — a changed .cpp may not rebuild; touch it
- [Split macro headers](build/split-macro-headers.md) — macros.hpp edit = 11 min rebuild; use narrow headers
- [ASAN for LLVM crashes](build/asan-build-for-llvm-crashes.md) — gdb can't unwind LLVM; -DSPP_SANITIZER=address
- [heaptrack needs mimalloc removed](build/heaptrack-needs-mimalloc-removed.md) — -DSPP_NO_MIMALLOC build + patchelf out libmimalloc
- [Stderr behind progress bar](build/spp-stderr-behind-progress-bar.md) — tr '\r' '\n' before grepping
- [Binary memory cap](build/spp-run-binary-memory-cap.md) — ulimit -v guard in cli.cpp; keep it
- [std tests can't see unreachable](build/std-tests-cannot-see-unreachable.md) — spp test is rel+LTO; virtual base, no prior call

## Areas (open the area's INDEX.md when the work touches it)
- [Toolchain](toolchain/INDEX.md) — GCC/clang C++20 module bugs and workarounds, LLVM wrapper module, linters (12)
- [CI and platforms](ci/INDEX.md) — workflow quirks, compiler caches, macOS libc++, MSVC bugs, cross-target blockers (16)
- [Type identity](types/INDEX.md) — TypeKey/stamps, aliases, variant order, TypeEq vs Assignable, Self (11)
- [Generics](generics/INDEX.md) — inference, variadic packs, sups and attach, overload resolution (19)
- [Analysis internals](analysis/INDEX.md) — scopes and the walk API, stages, cloning, errors, perf (20)
- [Language semantics](language/INDEX.md) — linear types, drops, borrows, case/patterns, closures, generators (23)
- [Codegen and runtime](codegen/INDEX.md) — coercion, layout, mangling, hardening keys, abort path (18)
- [Std and sibling repos](repos/INDEX.md) — SPP-STL and sppc: what lives where, prelude, link-time library checks (3)
