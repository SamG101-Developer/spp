# Upstream watch

Features and fixes in third-party projects that this compiler is waiting on, with what we do meanwhile and what to
change once they land. Scan it on each dependency bump (LLVM, GCC, clang, MSVC, CMake, CI actions), or every few months:
run each entry's check, update its status and "last checked" date, and move what has landed to "Resolved".

Each entry gives:

- **Library:** the project, and where the work happens.
- **Want:** what we are waiting for.
- **Why:** what it would let us do.
- **Meanwhile:** our workaround, and where it lives.
- **Status:** what is known, with dates.
- **Check:** how to tell whether it has landed.
- **When it lands:** what to change here.
- **Last checked:** when the status was last confirmed (from a note, or checked directly).

| #   | Library          | Want                                                            | Last checked |
| --- | ---------------- | --------------------------------------------------------------- | ------------ |
| U1  | LLVM             | New pass manager codegen pipeline on every target               | 2026-11-10   |
| U2  | GCC              | `extern "C++"` forward declarations across modules              | 2026-11-10   |
| U3  | GCC              | `lookup_mark` ICE on unqualified calls and cross-module friends | 2026-11-10   |
| U4  | GCC              | "Bad file data" reading a compiled module cluster               | 2026-11-10   |
| U5  | clang            | Implicit destructor ICE in an importer (`GetVTTParameter`)      | 2026-11-10   |
| U6  | MSVC STL         | `std::string + std::string_view` (P2591)                        | 2026-11-10   |
| U7  | MSVC             | Inline bodies from a GMF instantiated in importers              | 2026-09-11   |
| U8  | CMake            | `vs_link_exe` treating a leading `@` as a response file         | 2026-11-10   |
| U9  | ccache-action    | `append-timestamp: false`                                       | 2026-11-10   |
| U10 | Ubuntu toolchain | A current GCC release for the runner images                     | 2026-11-10   |
| U11 | ccache, sccache  | Caching C++20 module compiles from GCC                          | 2026-11-10   |
| U12 | sccache          | Caching C++20 module compiles from cl.exe                       | 2026-11-10   |
| U13 | LLVM             | PC-relative EH pointers on m68k under PIC                       | 2026-11-10   |
| U14 | CMake            | `import std` with clang-cl and the MSVC STL (CMake 4.5)         | 2026-11-10   |
| U15 | LLVM             | SafeStack checking a canary through the target's own route      | 2026-11-10   |
| U16 | LLVM             | AArch64 SLS indirect-call thunks named for Mach-O               | 2026-11-10   |
| U17 | qemu             | `getsockopt` of `SO_RCVTIMEO`/`SO_SNDTIMEO` under linux-user    | 2026-11-10   |
| U18 | qemu             | ppc32 rt sigreturn CFA missing `RT_SIGFRAME_ADJUST` (vDSO)      | 2026-11-10   |
| U19 | qemu             | loongarch64 sigreturn CFA register and return column (vDSO)     | 2026-11-10   |
| U20 | LLVM             | ARMv5 sub-word `__sync_*` results sign- vs zero-extended        | 2026-11-10   |
| U21 | LLVM             | m68k 16-bit PC-relative and PLT relocations in large programs   | 2026-11-10   |
| U22 | MSVC             | Implementation units see the interface's using-declarations     | 2026-11-08   |
| U23 | MSVC             | Templates keep using-declared names when instantiated elsewhere | 2026-11-08   |
| U24 | LLVM             | `Module::renumberMetadataForAssembly()` (LLVM 24)               | 2026-11-10   |
| U25 | LLVM             | `HasRunOnLoopT` out of an anonymous namespace (#233326)         | 2026-11-10   |
| U26 | LLVM             | KCFI type id kept in the function's atom on Mach-O (#241037)    | 2026-11-10   |

## U1. LLVM: the new pass manager's codegen pipeline on every target

- **Library:** LLVM (`llvm/lib/Target/*`, `llvm/include/llvm/Passes/CodeGenPassBuilder.h`).

- **Want:** `TargetMachine::buildCodeGenPipeline` (the new pass manager's codegen pipeline) implemented by every backend
  the compiler builds for, and the default for object emission.

- **Why:** object emission is the last use of the legacy pass manager here. With it, one pass manager drives both
  optimisation and codegen.

- **Meanwhile:** `codegen::EmitObjectFile` (`../sources/spp/codegen/llvm_target.cpp`) emits through
  `llvm::legacy::PassManager` and `addPassesToEmitFile`. See next-steps N5.6.

- **Status (2026-10-24):** the LLVM docs still say target-dependent codegen uses the legacy pass manager, and there is
  no published timeline or removal date. The base `buildCodeGenPipeline` answers "not overridden". On trunk it is
  implemented by:

  - X86 (`X86CodeGenPassBuilder.cpp`, since 2024-01, 56 commits, active);
  - WebAssembly (since 2026-07, active);
  - RISC-V (since 2026-08, a "skeleton");
  - NVPTX (since 2026-08);
  - MIPS and SPARC (new by 2026-11-07);
  - SystemZ (`bff8793c57a6`, 2026-11-09);
  - AMDGPU, BPF, Lanai and MSP430.

  None on trunk (2026-11-10): AArch64, ARM, PowerPC, LoongArch, M68k, Hexagon. Even on X86 it is experimental
  (`llc -enable-new-pm`). In the installed LLVM 23, only X86, AMDGPU, BPF, Lanai and MSP430 have it.

  LLVM 23 still registers 26 shared codegen passes as `DUMMY` (unported, so skipped), among them `machine-combiner`,
  `funclet-layout`, `cfguard-longjmp`, `machine-outliner` and GlobalISel; trunk is down to 13 (those five ported or
  gone). Nothing ships on it: clang 23 drives codegen through the legacy pass manager.

  LLVM 24 adds `llvm::runCodeGenPipeline` (`llvm/Passes/RunCodeGen.h`, `dca328efcdee`, 2026-09-03), which clang,
  flang and LTO now call. It takes the new pass manager where `TargetMachine::shouldDefaultToNewPM()` says so (only
  MSP430 and Lanai on 2026-11-10) and the legacy one otherwise, so the per-target split is kept upstream. Its legacy
  path also adds the TTI, `TargetLibraryInfo` and `RuntimeLibraryInfoWrapper` passes. `EmitObjectFile` adds them too
  since 2026-11-10 (byte-identical objects on 8 targets, std test binary included). Its `RuntimeLibraryInfoWrapper`
  call reads `TargetOptions::FloatABIType`/`ExceptionModel`/`EABIVersion`, which LLVM 24 removes: the bump has to
  replace it.

- **Check:** for each target in the cross-codegen list, does
  `llvm/lib/Target/<T>/<T>CodeGenPassBuilder.cpp` exist, or does `<T>TargetMachine.cpp` override
  `buildCodeGenPipeline`? Does `llvm/docs/NewPassManager.md` (Markdown since the LLVM 24 docs move) still say codegen
  uses the legacy pass manager (it did on 2026-11-10)? Locally:
  `nm -C libLLVM<T>CodeGen.a | grep "TargetMachine::buildCodeGenPipeline"`.

- **When it lands:** on the LLVM 24 bump, replace the body of `EmitObjectFile` with one `llvm::runCodeGenPipeline`
  call: each target moves to the new pass manager as LLVM flips its `shouldDefaultToNewPM`. Do not keep our own
  split by target. Once every target the compiler builds for is flipped, nothing is left to do.

- **Last checked:** 2026-11-10 (trunk target directories and `NewPassManager.md`; archives as of 2026-10-24).

- **Sources:** [NewPassManager docs](https://llvm.org/docs/NewPassManager.html),
  [pipeline construction RFC (2025)](https://discourse.llvm.org/t/rfc-codegen-new-pass-manager-pipeline-construction-design/84659),
  [CodeGenPassBuilder introduction (D83608)](https://reviews.llvm.org/D83608).

## U2. GCC: `extern "C++"` forward declarations across modules

- **Library:** GCC (C++ modules, `gcc/cp/module.cc`).
- **Want:** three bugs fixed, none of which clang has.
  1. A forward declaration merges only from a direct import: an intermediate module that does not redeclare the type
     gives "marked 'override', but does not override".
  1. An importer cannot redeclare a non-exported purview declaration whose definition is reachable but not visible (
     "conflicting declaration", then "not declared"). A GCC 16 regression: 14.2 to 15.2 accept it.
  1. GMF and purview declarations of one type mixed along an import chain break overrides.
- **Why:** the AST forward declarations could be non-exported (`extern "C++"` in the purview), so those names stop being
  exported, as wanted.
- **Meanwhile:** every AST forward declaration is `SPP_EXP_CLS` in the purview (`SPP_AST_COMMON_FWD_DECL`, `use()`),
  redeclared in every module on the chain, and the defining module imported directly where bug 2 bites. A declaration
  that looks unused in a header may be one of these; diff against `HEAD` before removing any.
- **Status:** reproduced with standalone modules (bugs 1 and 2). A Bugzilla-ready report for bug 2 was written to
  `docs/bug2/`, but it is no longer on disk and was never committed; whether it was filed, and its number, is not
  recorded. Bugzilla blocks automated fetches, so duplicates were not checked.
  **2026-11-10: bug 1 is fixed on GCC trunk** (17.0.0 20261018): with `GenericArgumentAst` dropped from
  `literal_ast.ixx`, the whole of `spp` builds and links under `-Werror` (in a scratch worktree). Bugs 2 and 3 were not
  tried (no repro on disk). Not tried with GCC 16.2, which CI pins, so the redeclarations stay for now.
- **Check:** remove one of the redeclarations that exist only for bug 1 (e.g. `GenericArgumentAst` in
  `literal_ast.ixx`) and build with the current GCC, using the project's `-Wall -Wextra -Werror`; grep for
  `warning` as well as `error`. A standalone repro would need writing again.
- **When it lands:** turn the AST forward declarations into non-exported `extern "C++"` purview declarations, and drop
  the redeclarations that exist only for bug 1. For bug 1 alone: once CI's GCC has the fix (trunk does, 16.2 untested).
- **Last checked:** 2026-11-10 (bug 1, GCC trunk 20261018).

## U3. GCC: `lookup_mark` ICE

- **Library:** GCC (`cp/tree.cc:2637`, `lookup_mark`).
- **Want:** no ICE ("internal compiler error: in lookup_mark") for (a) an unqualified call to a free function of the
  same module from a member function body, and (b) a friend function overloaded across modules.
- **Why:** calls could be written unqualified, and an overloaded function could be befriended.
- **Meanwhile:** such calls are fully qualified (`spp::analyse::scopes::PrecompiledTemplate` in `TypeRef::IsA`,
  `BindByName`/`ParamsOfGroup` in `TypeSymbol::AliasTargetId`, both in `symbols.cpp`); the `TypeKey` caches are public
  `_`-prefixed fields rather than befriended. If a tidy drops one of those qualifications and the build ICEs in
  `lookup_mark`, restore it.
- **Status:** not reduced to a standalone repro or reported, as far as the notes say. Since the 2026-10-06 renames
  the calls are `ParamsDeclaredBy`/`BindingsOfArgs` in `TypeSymbol::AliasTargetId`, and `PrecompiledTemplate` in
  `TypeRef::IsA`. **2026-11-10, GCC trunk 17.0.0 20261018:** half fixed. `AliasTargetId` builds unqualified;
  `IsA` still ICEs (`lookup_mark`, `cp/tree.cc:2638`), through ADL: `perform_koenig_lookup` ->
  `lookup_arg_dependent` -> `name_lookup::search_adl` -> `dedup`. Not tried with GCC 16.2 (CI's).
- **Check:** unqualify one of the calls above and build.
- **When it lands:** calls can drop their qualification where a style choice wants it.
- **Last checked:** 2026-11-10 (GCC trunk 20261018, scratch worktree).

## U4. GCC: "Bad file data" reading a compiled module cluster

- **Library:** GCC (C++ modules, reading `.gcm`).
- **Want:** no deterministic "failed to read compiled module cluster N: Bad file data" (and "failed to load pendings for
  'std::allocator'") for certain import graphs.
- **Why:** an interface could import what it needs rather than forward-declare around it.
- **Meanwhile:** a `use()` forward declaration instead of an interface import where only a pointer or reference is
  needed (`instance_key.ixx` and `CompNode`, 2026-10-02); AST-needing functions moved into a second TU
  (`substitution.cpp`, 2026-10-08).
- **Status:** not reduced or reported, as far as the notes say. **2026-11-10: gone on GCC trunk** (17.0.0 20261018):
  with the import added to `type_key.ixx` (the 2026-10-09 rename of `instance_key.ixx`), every importer compiles and
  `spp` links, with no "Bad file data". Not tried with GCC 16.2 (CI's), so the workarounds stay for now.
- **Check:** add `import spp.analyse.scopes.comp_key;` back to `type_key.ixx` and build.
- **When it lands:** the workarounds above can go back to plain imports, once CI's GCC is past it.
- **Last checked:** 2026-11-10 (GCC trunk 20261018, scratch worktree).

## U5. clang: implicit destructor ICE in an importer

- **Library:** clang (codegen of an implicit destructor for a type from another module; stack
  `destroyCXXObject` -> `GetVTTParameter` -> `castFromDeclContext`).
- **Want:** an importer emitting an exported aggregate's implicit destructor does not segfault. Seen with clang 23.1.2
  on `PlaceAt` (`regions.ixx`, an aggregate holding an `Opt<Place>`) once `Place` gained a defaulted `<=>`.
- **Why:** special members could be left implicit.
- **Meanwhile:** `PlaceAt` declares all its special members and defines them `= default` in `regions.cpp`, inside
  `SPP_MOD_BEGIN`/`SPP_MOD_END`. The same stack on another `SPP_EXP_CLS` type gets the same fix.
- **Status:** a modules merge bug; GCC is fine. Not recorded as reported. **Still crashes with clang 23.1.5**
  (2026-11-10): with the copy, move, assignment and destructor left implicit (both constructors still out-of-line),
  four importers segfault on `PlaceAt::~PlaceAt` with the same stack: `symbols.cpp`, `defer_statement_ast.cpp`,
  `loop_control_flow_statement_ast.cpp`, `pattern_guard_ast.cpp`.
- **Check:** with the clang tree (see the local clang coverage build), drop `PlaceAt`'s out-of-line special members and
  build `spp` with `ninja -k 0` (more than one importer crashes).
- **When it lands:** remove the out-of-line special members.
- **Last checked:** 2026-11-10 (clang 23.1.5, scratch worktree).

## U6. MSVC STL: `std::string + std::string_view`

- **Library:** Microsoft STL (P2591, C++26).
- **Want:** `operator+` for `std::string` and `std::string_view`, as libstdc++ already has.
- **Why:** a `Str + StrView` chain builds on Windows without converting first.
- **Meanwhile:** convert the view first with `AsStr(StrView) -> Str` (in `semantic_error.cpp`; `Str(x)` would trip GCC's
  `-Wuseless-cast`), and keep macros that expand to a `+` chain (`INLINE_INFO` and friends) parenthesised.
- **Status:** not implemented: the MSVC STL's `main` still defines `__cpp_lib_string_view` as `201803L` (P2591 bumps
  it to `202403L`), and no issue or PR names P2591 (2026-11-10).
- **Check:** the MSVC STL changelog or status page for P2591, or a one-line test on the Windows runner.
- **When it lands:** `AsStr` at those splices can go (keep the macro parentheses).
- **Last checked:** 2026-11-10 (STL `main`).

## U7. MSVC: inline bodies from a global module fragment, in importers

- **Library:** MSVC (seen with VS 18 / 14.51.36231, windows-2025 runner).
- **Want:** inline function bodies included in a module's GMF instantiate correctly in importers. Failed in CLI11's
  `StringTools_inl.hpp` (`iss >> word`): "`_Ok` uses undefined class `std::basic_istream<...>::sentry`".
- **Why:** third-party header-only code can stay header-only in its wrapper module.
- **Meanwhile:** `#define CLI11_COMPILE` in `../headers/wrappers/cli11.ixx` (declarations only), with
  `../sources/wrappers/cli11.cpp` compiling every `CLI/impl/*_inl.hpp` in a plain TU. CLI11's templates still instantiate
  in importers, so a sentry error from one of those would be this bug again.
- **Status:** began with a runner toolset update, not a change here. Not recorded as reported.
- **Check:** drop `CLI11_COMPILE` and the plain TU, and build on the Windows runner.
- **When it lands:** CLI11 can be included header-only again.
- **Last checked:** 2026-09-11.

## U8. CMake: `vs_link_exe` and a leading `@`

- **Library:** CMake (`cmcmd.cxx`, `cmcmd::VisualStudioLink`).
- **Want:** `cmake -E vs_link_exe` not to read an argument beginning with `@` as an nmake response file. CMake's own
  `import std` library is `@cmake_cxx_std.lib`; it was read as a response file, expanded to nothing, and never linked,
  giving `LNK2019` for `std::strong_ordering::less` and the like.
- **Why:** archives could stay in the build root on Windows.
- **Meanwhile:** `set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")` under `if (MSVC)` in the root
  CMakeLists, and `-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=lib` in `install-small-libs.sh` for the dependency builds.
- **Status:** still present in CMake 4.4.0 through 4.4.5, in v4.5.0-rc3 and on `master` (2026-11-10): `cmcmd.cxx`
  exempts only `@CMakeFiles` and `@.o/`.
- **Check:** `cmcmd::VisualStudioLink` in the current CMake source, or the Windows build with the override removed.
- **When it lands:** remove the archive directory override (both places).
- **Last checked:** 2026-11-10 (CMake v4.4.5, v4.5.0-rc3 and `master` source).

## U9. ccache-action: `append-timestamp: false`

- **Library:** `hendrikmuhs/ccache-action` (`restore.ts`).
- **Want:** `append-timestamp: false` honoured. In v1.2.24 the input is tested as a string, so `"false"` is truthy and a
  trailing `-` is appended to the key anyway.
- **Why:** cache keys without a timestamp.
- **Meanwhile:** the default (timestamped keys) and no sha in the key; `prune-caches.sh` groups `ccache-`/`sccache-`
  keys with `:3`, as the ISO date splits into three dash segments.
- **Status:** broken in v1.2.24, still the latest release, and on `main` (`core.getInput`, 2026-11-10).
- **Check:** on each ccache-action bump, read `restore.ts` for the string test.
- **When it lands:** switching to `append-timestamp: false` is possible; keep the prune segment count in step with the
  key shape.
- **Last checked:** 2026-11-10.

## U10. Ubuntu toolchain: a current GCC for the runner images

- **Library:** `ppa:ubuntu-toolchain-r/test`, and Ubuntu's own packages.
- **Want:** a GCC 16 release (with the fix for PR125768, the imported-incomplete-type modules ICE in `symbols.cpp`)
  for the Ubuntu versions the GitHub runner images use.
- **Why:** CI could take GCC from apt rather than Homebrew.
- **Meanwhile:** `../.github/scripts/setup/install-gcc-linux.sh` installs GCC from Homebrew, pinned by `[pin.gcc]`
  in `../.github/dependencies.toml` (matched against `g++ -dumpfullversion`). It installs Homebrew's binutils too (its GCC
  emits `.base64`, which the runner's binutils 2.42 rejects) and strips Homebrew's specs-file loader and rpath
  additions. The clang jobs still take libstdc++ from the PPA.
- **Status (2026-11-10):** the PPA's newest gcc-16 for noble is still a 2026-03-15 trunk snapshot, before the PR125768 fix. GCC 16.2
  packaging exists only in Ubuntu 26.10 and Debian sid, neither of which has a runner image. The `ubuntu-26.04` image
  (GA 2026-09-17; `ubuntu-latest` moves to it in November 2026, runner-images #14748) does not help: its default GCC is
  15.2 (13.4, 14.3, 15.2 installed), and 26.04's archive gcc-16 is a 2026-03-22 snapshot. CI pins `ubuntu-24.04`, so
  the `ubuntu-latest` move does not touch it.
- **Check:** the PPA's gcc-16 package date for the runner's Ubuntu release, and whether GitHub ships an image on a
  release with GCC 16.2 or later.
- **When it lands:** go back to apt for GCC; the Homebrew binutils and specs workarounds go with it.
- **Last checked:** 2026-11-10 (Launchpad API, runner-images Ubuntu 26.04 readme).

## U11. ccache, sccache: caching GCC's C++20 module compiles

- **Library:** ccache (#1252, draft PR #1523) and sccache (#2630).
- **Want:** GCC compiles with `-fmodules-ts` cached, with the imported and produced `.gcm` files hashed and restored.
- **Why:** the GCC jobs build every TU with modules, so the compiler cache does nothing for them.
- **Meanwhile:** none needed for correctness. ccache 4.14.3 counts these compiles as unsupported and sccache 0.19.0
  refuses them, so they always compile fresh. The launcher stays on for GCC so a release that adds support takes effect.
- **Status:** ccache#1523 had GCC 14 caching working but stalled in 2026-02. Its author says the compilers need upstream
  patches first: their P1689 output leaves too much out. sccache#2630 has no PR. sccache#2516's author calls GCC's
  module handling "a mess" that needs an entirely different approach.
- **Check:** the ccache and sccache release notes for GCC module support, and ccache#1523 and sccache#2630. Confirm on
  CI: the ccache stats on a second GCC build should show hits rather than `unsupported_compiler_option`.
- **When it lands:** bump `[pin.ccache]`/`[pin.sccache]`. Before trusting it, check that a change to a module interface
  alone makes its importers miss: export `f(int)`, compile an importer through the cache, change it to
  `f(int, int = 0)`, rebuild the module and the importer, and `nm` must show the new mangling (`_ZW1m1fii`).
- **Last checked:** 2026-11-10 (ccache 4.14.3, out 2026-11-08, has nothing on modules in its notes; sccache 0.19.0 still
  latest; #1523 untouched since 2026-02, sccache#2630 since 2026-03).

## U12. sccache: caching cl.exe's C++20 module compiles

- **Library:** sccache (#2629, PR #2934).
- **Want:** cl.exe module compiles (`/interface`, `/ifcOutput`, `/reference`, `@<file>.modmap`) cached, with the
  imported IFCs hashed.
- **Why:** the Windows MSVC jobs (x64 and, since 2026-11-08, arm64) get a 0% hit rate. The clang-cl job is already cached (sccache #2876, in 0.19.0).
- **Meanwhile:** none needed for correctness. sccache 0.19.0 refuses every cl.exe module flag, so these compile fresh.
  Do not move Windows to ccache: before ccache PR #1837 it caches MSVC module compiles and can return stale objects.
- **Status:** PR #2934 opened 2026-10-23, still open, with no reviews (its only comment is Codecov's), last touched
  2026-11-09 (2026-11-10). `/headerUnit` would stay uncacheable. ccache#1837 (stop ccache caching MSVC module compiles)
  is still open, last touched 2026-11-04.
- **Check:** the sccache release notes for #2934, then the MSVC job's sccache stats on a second build.
- **When it lands:** bump `[pin.sccache]` and confirm that a module-interface-only change makes importers miss.
- **Last checked:** 2026-11-10 (sccache 0.19.0 still latest).

## U13. LLVM: PC-relative EH pointers on m68k under PIC

- **Library:** LLVM (`TargetLoweringObjectFileELF::initialize`, `llvm/lib/CodeGen/TargetLoweringObjectFileImpl.cpp`).
- **Want:** a `case Triple::m68k` in its per-architecture switch, so a position-independent m68k build writes its
  personality, LSDA and type-info pointers PC-relative (through `DW.ref` for the personality), as x86, ppc, riscv,
  s390x, sparc and loongarch do.
- **Why:** with no case, m68k falls through to absolute pointers. The FFI unwind guard gives every function that calls
  `!ffi` a personality and an LSDA, so the PIE link fails under `-z text` ("read-only segment has dynamic
  relocations", from `R_68K_32` in `.eh_frame`).
- **Meanwhile:** `[binary.codegen] ffi_unwind_guard` (on by default) refuses m68k, naming the key
  (`TargetHasPcRelEhPointers` in `../sources/spp/codegen/llvm_harden_codegen.cpp`), and the m68k cross-codegen entry
  turns it off. m68k programs build without the guard: a foreign exception still aborts, without the named report.
- **Status (2026-10-24):** absent on trunk (`299c65b98b02`). A one-line fix and an `eh-pic-encoding.ll` test were
  written for an upstream PR, [llvm/llvm-project#235347](https://github.com/llvm/llvm-project/pull/235347), opened
  2026-10-24, merged 2026-10-25 as `62e4899157d6`. As of 2026-11-10 it is on `main` (LLVM 24) only: not on
  `release/23.x` nor in 23.1.5 (the newest release), and no backport (`/cherry-pick`) has been requested.
- **Check:** whether #235347 has merged and into which release; then
  `grep -n "case Triple::m68k" llvm/lib/CodeGen/TargetLoweringObjectFileImpl.cpp` in the installed LLVM's
  release branch, or emit m68k with the key on and `readelf -r spp.o` for `R_68K_32` in `.rela.eh_frame`.
- **When it lands:** once spp builds against an LLVM with the fix, delete `TargetHasPcRelEhPointers` and its refusal,
  drop `ffi_unwind_guard` from the m68k entry's `off` in `_cross_codegen.yaml`, and decide whether the key stays (S20.1
  had it keyless: it protects a language guarantee).
- **Last checked:** 2026-11-10.

## U14. CMake: `import std` with clang-cl and the MSVC STL

- **Library:** CMake (`Modules/Compiler/Clang-CXX-CXXImportStd.cmake`).
- **Want:** `CXX_MODULE_STD` working for Clang targeting the MSVC ABI (clang-cl with the MSVC STL). CMake 4.4 accepts
  only libc++ and libstdc++ for Clang, so configure fails with "The "CXX_MODULE_STD" property on target ... requires
  toolchain support ... Only `libc++` and `libstdc++` are supported", in spp and in every sibling library (ColEx first).
- **Why:** the `windows clang-cl` lane could build at all.
- **Meanwhile:** the lane is advisory: `advisory: true` on its `pr.yaml` matrix entry, which `_lane.yaml` turns into
  `continue-on-error` on its build and test jobs, and it is outside `ci-success`. `select-windows-compiler.sh` already
  points it at the pinned LLVM 23's `clang-cl.exe`, not the image's Clang 20 on PATH.
- **Status (2026-10-25):** fixed upstream by commit `1610aa22f4` ("Clang: Add support for `import std` when targeting
  the MSVC ABI", 2026-07-04, closes CMake #26761): Clang on the MSVC ABI goes through the MSVC module logic. Not in any
  4.4.x (4.4.4 is the latest); in v4.5.0-rc1 (2026-10-15). 4.4 and 4.3 each took about four weeks from rc1 to final,
  so 4.5.0 is expected around mid-November 2026 (an estimate, not an announced date). v4.5.0-rc3 came out
  2026-11-04 and keeps the MSVC-frontend include; still no final, and no rc4 (2026-11-10; 4.4.5 is the newest 4.4.x). Whether clang 23 then compiles
  the MSVC STL's `std.ixx` cleanly is untested.
- **Check:** `gh api repos/Kitware/CMake/releases --jq '.[0:3][].tag_name'` for a final 4.5.0; the top of
  `Clang-CXX-CXXImportStd.cmake` at that tag should include `Compiler/MSVC-CXX-CXXImportStd` for the MSVC frontend.
- **Pinned to the RC (2026-11-07):** `[pin.cmake]` is `4.5.0-rc3`, installed by `lukka/get-cmake` at its `v4.5.0-rc3`
  tag (its built-in release catalog decides which versions it can install: v4.4.4's ended at 4.4). Every lane builds
  with it; `cmake_minimum_required` stays 4.4.0, so local 4.4 builds still configure.
- **When it lands:** bump `[pin.cmake]` in `../.github/dependencies.toml` to 4.5.0 and `lukka/get-cmake` (in
  `setup-toolchain/action.yaml` and `_cppcheck.yaml`) to a tag whose catalog has 4.5.0 (the sibling libraries are built
  with the same CMake), run the clang-cl lane, and once it is green drop `advisory: true` and the "(advisory)" from its
  name in `pr.yaml`, and consider adding it to `ci-success`. Also recheck U8 on the new CMake.
- **Last checked:** 2026-11-10 (CMake tags, `Clang-CXX-CXXImportStd.cmake` at v4.5.0-rc3).
- **Sources:** [commit 1610aa22f4](https://github.com/Kitware/CMake/commit/1610aa22f4),
  [cmake-cxxmodules(7)](https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html),
  [CMake Discourse: import std on Windows](https://discourse.cmake.org/t/import-std-doesnt-work-on-windows/14268).

## U15. LLVM: SafeStack's canary check on MSVC

- **Library:** LLVM (`SafeStack::checkStackGuard`, `llvm/lib/CodeGen/SafeStack.cpp`).
- **Want:** the SafeStack pass checking a function's canary the way the target's stack protector does, so MSVC
  triples call `__security_check_cookie` (as `StackProtector` does through `getSSPStackGuardCheck`), not only
  `STACKPROTECTOR_CHECK_FAIL`.
- **Why:** SafeStack takes over the canary of every `ssp`/`sspstrong` function it splits, and its check always calls
  `__stack_chk_fail`, which MSVC triples do not have. With `[memory.stack] protect` and `split` both on, every protected
  function fails with "no libcall available for stackprotector check fail", and no object is written.
- **Meanwhile:** `CheckTargetSupport` in `../sources/spp/cli.cpp` refuses the pair on a target without
  `__stack_chk_fail` (`HasSafeStackCanary` in `../sources/spp/codegen/llvm_target.cpp`), naming both keys.
  The Windows lanes pass `off: memory.stack.split` (`pr.yaml`, `post_merge.yaml`), keeping the canary. (Whether sppc's unsafe stack works on
  Windows at all is open separately: sppc `PORTING.md` section 8.)
- **Status (2026-11-10):** unchanged on trunk: `checkStackGuard` still errors when `STACKPROTECTOR_CHECK_FAIL` is
  unsupported (`SafeStack.cpp`'s two commits since 2026-11-01 do not touch it). No upstream issue found.
- **Check:** in `SafeStack.cpp`, whether `checkStackGuard` still errors when `STACKPROTECTOR_CHECK_FAIL` is
  unsupported; or `llc -mtriple=x86_64-pc-windows-msvc` on a function with `sspstrong safestack`.
- **When it lands:** delete `HasSafeStackCanary`/`TargetHasSafeStackCanary`, the refusal and its Todo, the two
  `TestTargetSupport` canary tests, and `memory.stack.split` from the Windows lanes' `off` in `pr.yaml` and
  `post_merge.yaml`.
- **Last checked:** 2026-11-10.

## U16. LLVM: AArch64 SLS thunks on Mach-O

- **Library:** LLVM (`SLSHardeningInserter::convertBLRToBL`, `llvm/lib/Target/AArch64/AArch64SLSHardening.cpp`).
- **Want:** the `bl` to an SLS indirect-call thunk naming the thunk by its mangled symbol, so on Mach-O it carries the
  `_` global prefix like the thunk's definition.
- **Why:** with `+harden-sls-blr` (and `+harden-sls-nocomdat`, as Mach-O needs), the thunk is an IR function emitted
  as `___llvm_slsblr_thunk_xN`, but the call site uses `Context.getOrCreateSymbol(ThunkName)`, the raw
  `__llvm_slsblr_thunk_xN`, which nothing defines. Every macOS arm64 link fails with "Undefined symbols ...
  \_\_llvm_slsblr_thunk_x10". clang's `-mharden-sls=blr` on Darwin hits the same. ELF is unaffected (no prefix). LLVM's
  SLS tests have no Darwin case.
- **Meanwhile:** `TargetHasSlsHardening` in `../sources/spp/codegen/llvm_harden_codegen.cpp` is false for AArch64
  Mach-O, so `[binary.codegen] sls` (on by default) is refused there, naming the key; the `retbr` half, which works,
  goes with it, since the key covers both. The macOS test calls pass `off: sls` (`pr.yaml`, `post_merge.yaml`).
- **Status (2026-11-10):** unchanged on trunk (`convertBLRToBL` still calls `Context.getOrCreateSymbol(ThunkName)`;
  the file's recent commits are inserter refactors, and no PR names the Mach-O thunk). Reproduce: `llc -mtriple=aarch64-apple-macos -filetype=obj` on a function doing `call void %p()` with `"target-features"="+harden-sls-blr,+harden-sls-nocomdat"`;
  `llvm-nm` shows `t ___llvm_slsblr_thunk_x0` and `U __llvm_slsblr_thunk_x0`. A fix is drafted in the local
  llvm-project working tree (2026-10-26, not yet a PR): `convertBLRToBL` takes the call's symbol from the thunk
  function (`MF.getTarget().getSymbol`) instead of `Context.getOrCreateSymbol(ThunkName)`, with the test
  `llvm/test/CodeGen/AArch64/speculation-hardening-sls-blr-macho.ll`. It fails on unpatched LLVM 23 and passes with
  the patch, alongside the 8 existing AArch64 SLS tests. Rechecked on `main` `98407454f4a9` (2026-11-10): applies
  cleanly, the new test plus all 8 SLS tests and `kcfi.ll`/`setjmp-bti.ll` pass, and the new test fails without the
  fix. Opened as [llvm/llvm-project#240915](https://github.com/llvm/llvm-project/pull/240915) (2026-11-10,
  branch `users/samg101-developer/apple-sls-fix`); it must merge before `release/24.x` branches (about mid-January
  2027\) to be in LLVM 24.
- **Check:** whether #240915 has merged and into which release; then the same `llc` + `llvm-nm`: one defined name, no
  `U` thunk.
- **When it lands:** restore the AArch64 Mach-O case in `TargetHasSlsHardening`, with `+harden-sls-nocomdat` added to
  its features on Mach-O again (Mach-O has no COMDATs), and drop `off: sls` from the macOS test calls.
- **Last checked:** 2026-11-10.

## U17. qemu: `getsockopt` of `SO_RCVTIMEO`/`SO_SNDTIMEO` under linux-user

- **Library:** qemu (`do_getsockopt`, `linux-user/syscall.c`, the `get_timeout:` branch).
- **Want:** the NULL-`optval` special case there to test `!optval_addr`, as every other case that commit
  `08dc3e240fc0` ("linux-user: Allow getsockopt() with NULL optval address", first in 11.1.0) converted does.
- **Why:** with the inverted test, any `getsockopt` of a socket timeout with a real buffer succeeds, copies nothing and
  reports length 0. The cross-codegen job's unit-test step (`.github/scripts/test/cross-unit-tests.sh`) runs the
  standard library's tests under qemu for every qemu target, and
  `std::tst::net::{tcp,udp}::test_a_{read,write}_timeout_reads_back_as_it_was_set` fail there (550/554 on aarch64);
  on some targets they pass by luck, the buffer still holding the value just set (see
  `notes/ci/qemu-getsockopt-timeout-empty.md`).
- **Meanwhile:** CI's qemu-user is a static release of our fork, `SamG101-Developer/qemu` (`spp-patches`: qemu master
  plus this fix and U18-U19, rebased weekly by its Sync workflow; a `spp-*` tag builds and checks the release),
  pinned in `[pin.qemu-user]` and in sppc's `_build.yaml`. Under it the four tests pass.
- **Status (2026-11-10):** unchanged in qemu master (`723622bedb0b`) and in 11.1.3, the newest release. The fix is
  one character (`if (optval_addr)` to `if (!optval_addr)`, `Fixes: 08dc3e240fc0`, `Cc: qemu-stable`):
  `.triages/tickets/0003-linux-user-Fix-inverted-NULL-check-in-getsockopt-for-SO_RCVTIMEO.patch`, with the GitLab
  issue text in `.triages/tickets/qemu-getsockopt-timeout.md`. Carried in the fork; its Release build's check
  (`.github/spp/check-fixes.sh`) reads back `len=16` under the patched qemu-aarch64 (2026-10-26). Upstream
  issue not yet filed.
- **Check:** a guest program that sets `SO_RCVTIMEO` to 250 ms and reads it back gets optlen 16 and about 250000 us
  (not optlen 0) under qemu-user built from qemu master; or the four tests pass on every qemu target in the
  cross-codegen job.
- **When it lands:** nothing in the fork: once qemu master has it, the weekly Sync drops the patch by itself. Once
  U17-U19 are all in a qemu release, retire the fork: move `[pin.qemu-user]` and sppc's `_build.yaml` back to Debian's
  newest `qemu-user` deb (restoring the Debian section of `refresh-pins.sh` and `install-qemu-linux.sh` from git
  history), and delete the notes.
- **Last checked:** 2026-11-10.

## U18. qemu: the ppc32 rt sigreturn CFA in the linux-user vDSO

- **Library:** qemu (`linux-user/ppc/vdso.S`, `__kernel_sigtramp_rt32`; `RT_SIGFRAME_ADJUST` in
  `linux-user/ppc/signal.c`).
- **Want:** the trampoline's CFA to include `RT_SIGFRAME_ADJUST`, the 16 bytes `setup_rt_frame()` puts the 32-bit rt
  sigframe above the dummy frame, as Linux does.
- **Why:** without it the CFA is `r1 + 384` while the mcontext is at `r1 + 400`, so any unwind out of a signal handler
  under qemu-ppc reads every register 16 bytes off: glibc's `backtrace()` crashes in the handler, and sppc's crash
  reporter never reaches the faulting frame. qemu-ppc64 is fine (its adjustment is 0). Since e34136d93059
  ("linux-user/ppc: Add vdso"), qemu 8.2.
- **Meanwhile:** carried in our qemu fork with U17, which CI uses; its Release check unwinds 6 frames out of a handler
  under the patched qemu-ppc (2026-10-26). sppc's `_sppc_bt_print` also prints the fault pc alone as `#0`
  when an unwinder never reaches it (`../sppc/.triages/notes/ci/qemu-ppc32-vdso-sigframe-cfa.md`).
- **Status (2026-11-10):** unchanged in qemu master (`723622bedb0b`; `ppc/vdso.S` untouched since 2025-09). No upstream
  issue yet. The patch moves
  `RT_SIGFRAME_ADJUST` into `vdso-asmoffset.h` and adds it to the CFA (`Fixes: e34136d93059`, `Cc: qemu-stable`);
  `vdso-32.so` must be regenerated from it:
  `.triages/tickets/0001-linux-user-ppc-Include-RT_SIGFRAME_ADJUST-in-the-32-bit-rt-sigreturn-CFA.patch`, with the
  GitLab issue text in `.triages/tickets/qemu-ppc32-rt-sigreturn-cfa.md`. Upstream issue not yet filed.
- **Check:** the issue's `sigbt.c`, built with `powerpc-linux-gnu-gcc`, prints a backtrace through
  `__kernel_sigtramp_rt32` under qemu-ppc built from qemu master, instead of dying with SIGSEGV.
- **When it lands:** as U17. Then the sppc fallback can stay (it is right on any unwinder that stops short), but drop
  the sppc note.
- **Last checked:** 2026-11-10.

## U19. qemu: the loongarch64 sigreturn CFA and return column in the linux-user vDSO

- **Library:** qemu (`linux-user/loongarch64/vdso.S`, `__vdso_rt_sigreturn`).
- **Want:** two riscv numbers copied into its CFI corrected: the CFA register 3 (`$sp`), not 2 (`$tp` on LoongArch; `sp`
  on riscv), and the return column 72, GCC's `DWARF_ALT_FRAME_RETURN_COLUMN` for LoongArch, not 64 (riscv's; on
  LoongArch it is `fcc0`, a 1-bit register).
- **Why:** the signal frame's CFA is taken from the thread pointer, so any unwind out of a signal handler under
  qemu-loongarch64 faults (glibc's `backtrace()`, libgcc's `_Unwind_Backtrace`). With only the CFA fixed, libgcc reads
  the return address through `_Unwind_GetGR` on column 64, which it sizes at one byte, and aborts
  (`libgcc/unwind-dw2.c`); glibc's fatal message goes to `/dev/tty`, so in CI the run just dies (exit 134). Since 00cc2934b2f0
  ("linux-user/loongarch64: Add vdso"), qemu 8.2.
- **Meanwhile:** carried in our qemu fork with U17, which CI uses. With the CFA alone, its Release check showed the
  abort above; with both halves (`vdso.so` rebuilt: llvm-dwarfdump shows return column 72, CFA `R3 +304`; binutils
  2.44's readelf stops at that CIE), it unwinds 7 frames out of a handler under the patched qemu-loongarch64
  (2026-10-26). sppc's crash reporter falls back to printing the fault pc, as for U18; its loongarch64 build is
  `required: false` in `../sppc/.github/workflows/_build.yaml`.
- **Status (2026-10-26):** reported as https://gitlab.com/qemu-project/qemu/-/work_items/4743 (2026-10-23, open).
  Still open on 2026-11-10, with one comment (last activity 2026-10-28), and master's `vdso.S` still has
  `.cfi_def_cfa 2` and `.cfi_return_column 64`. The issue proposes only the CFA half; the patch has both (`Resolves:` that issue, `Fixes: 00cc2934b2f0`,
  `Cc: qemu-stable`; `vdso.so` must be regenerated):
  `.triages/tickets/0002-linux-user-loongarch64-Use-sp-not-tp-for-the-sigreturn-CFA.patch`.
- **Check:** the issue's reproducer prints `backtrace: 7 frames` under qemu-loongarch64 built from qemu master; or
  the issue is closed by a commit.
- **When it lands:** as U17; then consider making sppc's loongarch64 build required again.
- **Last checked:** 2026-11-10.

## U20. LLVM: ARMv5's sub-word `__sync_*` results are taken as zero-extended

- **Library:** LLVM's ARM backend (the libcalls it makes for atomics on a target with no atomic instructions), against
  libgcc's `libgcc/config/arm/linux-atomic.c`.
- **Want:** a 1- or 2-byte `__sync_val_compare_and_swap_N` (and the other sub-word `__sync_*`) result treated as libgcc
  returns it: sign-extended, as it declares them with `signed char` and `short` (lines 249-250), not zero-extended.
- **Why:** ARMv5 (`arm-linux-gnueabi`) has no atomic instructions, so LLVM lowers an atomic `i8` load to
  `__sync_val_compare_and_swap_1(p, 0, 0)` and compares the result as if zero-extended; libgcc hands back
  `0xffffffc8` for 200, so any byte of 128 or more misreads. `std::tst::threading::atomic:: test_an_atomic_of_a_narrower_width` fails on arm-linux-gnueabi (553/554). gcc's own callers agree with libgcc.
- **Meanwhile:** nothing; that one test stays red on arm-linux-gnueabi. A workaround was considered and not taken:
  sppc exporting zero-extending sub-word `__sync_*`, which, linked ahead of libgcc, would win.
- **Status (2026-10-27):** not reported. Reproduce with no spp: a C file doing
  `unsigned char a[4] = {200}; __sync_val_compare_and_swap(&a[0], 0, 0)`, built with `clang --target=armv5te-linux-gnueabi --sysroot=/usr/arm-linux-gnueabi -O1 -c`, linked with `arm-linux-gnueabi-gcc`, run
  under qemu-arm: it reads 4294967240; built with `arm-linux-gnueabi-gcc -march=armv5te` it reads 200.
  On trunk, llvm/llvm-project#238972 (merged 2026-11-04, LLVM 24) now marks the i8/i16 operands and results of the sized
  atomic libcalls `zeroext`, and lets callers rely on the extended result. If that covers the `__sync_*` calls, it
  writes the zero-extension assumption into the signature, so a fix would have to make it `signext` for ARM Linux. Not
  tried on trunk.
- **Check:** that reproducer reads 200 with the newest clang; or the test passes on arm-linux-gnueabi.
- **When it lands:** nothing to undo; the test goes green on the LLVM bump.
- **Last checked:** 2026-11-10 (trunk commits only; the reproducer was last run 2026-10-27).

## U21. LLVM: m68k reaches data and calls through 16-bit relocations

- **Library:** LLVM's M68k backend.
- **Want:** 32-bit displacements (the 68020's `(bd,PC)` addressing, or a `GOT32`/`PLT32` sequence) where a 16-bit one
  cannot reach, at least for `-mcpu=M68020` and up, as gcc does.
- **Why:** LLVM addresses data and calls with `R_68K_PC16`, `R_68K_GOT16` and `R_68K_PLT16`, so a program whose text
  is larger than they reach does not link ("relocation truncated to fit"). Hello world links; the standard library's
  unit-test binary does not (11 relocations, with `-latomic` linked). Tried on the test binary's IR with `llc-23`:
  `-mcpu=M68020`, `-code-model=large` and `medium`, and `-relocation-model=static`; all leave the same 11.
- **Meanwhile:** the cross-codegen job's m68k unit-test step fails at link; its hello-world run still links and runs.
  The missing `__atomic_*` functions it also hit are fixed in spp: the link adds `-latomic` when the object imports
  them (`ImportsAtomicLibcalls`, `../sources/spp/compiler/compiler_boot.cpp`).
- **Status (2026-11-10):** not reported; unchanged in LLVM 23. The M68k backend's trunk commits since 2026-10-27 are
  cleanups (carry handling, options in TableGen); none touches relocations or addressing.
- **Check:** `spp test --all-libs --target m68k-unknown-linux-gnu` links.
- **When it lands:** nothing to undo; the m68k unit-test step goes green on the LLVM bump.
- **Last checked:** 2026-11-10 (trunk commits only; the link was last tried 2026-10-27).

## U22. MSVC: a module implementation unit misses the interface's using-declarations

- **Library:** MSVC (VS 18 / 14.51.36231, windows-2025 runner). Developer Community 11179619.
- **Want:** a module implementation unit sees the non-exported `using ns::X;` declarations of its primary interface,
  as GCC and clang do.
- **Why:** `use(ns, X)` in an interface would then serve its `.cpp` too.
- **Meanwhile:** every module `.cpp` repeats the names it uses in a `use_alias` block, marked
  `MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL` (191 files). See `notes/ci/msvc-interface-names-not-inherited.md`.
- **Status (2026-11-08):** filed. Repro: `tickets/msvc-impl-unit-misses-interface-using-decl/`. The
  windows-2025-vs2026 image moved to MSVC 14.51.36247 (image 20261103), not yet tried (2026-11-10).
- **Check:** `build.bat` in the repro folder prints `ok` with the runner's toolset.
- **When it lands:** the marked `use_alias` lines a `.cpp` only needs for its interface's names can go.
- **Last checked:** 2026-11-08.

## U23. MSVC: a template loses using-declared names when another module instantiates it

- **Library:** MSVC (VS 18 / 14.51.36231, windows-2025 runner). Developer Community 11179620. Possibly related:
  11164692 (a using-directive in a GMF header's template, lost in an importer).
- **Want:** a class or function template's non-dependent names, found through a non-exported `using ns::X;`, stay
  bound to the definition when another module instantiates it.
- **Why:** templates in interfaces could use the short names like the rest of the interface.
- **Meanwhile:** those names are qualified inside the templates, marked `MSVC_DEVCOM_11179620_TEMPLATE_USING_DECL`
  (`comp_time_intrinsics.ixx`, `inner_scope_ast.ixx`).
- **Status (2026-11-08):** filed. Repro: `tickets/msvc-template-instantiation-misses-using-decl/`. Not yet tried on
  MSVC 14.51.36247 (2026-11-10).
- **Check:** `build.bat` in the repro folder prints `ok` with the runner's toolset.
- **When it lands:** the marked qualifications can be shortened again.
- **Last checked:** 2026-11-08.

## U24. LLVM: `Module::renumberMetadataForAssembly()` before printing IR

- **Library:** LLVM 24 (release notes, "Changes to the LLVM IR").
- **Want:** `Module::renumberMetadataForAssembly()`, needed once LLVM 24 gives metadata persistent IDs: a client that
  calls `Module::print()` directly no longer gets the `!N` numbers renumbered for it.
- **Why:** without the call, the `.ll` artefacts keep valid IR but may number and order metadata differently, which
  makes before/after `out/llvm/**/*.ll` diffs noisy (`notes/build/spp-ir-diff-verification.md`).
- **Meanwhile:** nothing to do: LLVM 23 (23.1.5) has no such function and `print()` still renumbers.
- **Status (2026-11-10):** in LLVM 24's in-progress release notes; not in `/usr/lib/llvm-23/include/llvm/IR/Module.h`.
- **Check:** `grep renumberMetadataForAssembly $(llvm-config --includedir)/llvm/IR/Module.h`.
- **When it lands:** on the LLVM 24 bump, call it on the module just before `print()` at both sites in
  `compiler_boot.cpp` (each module's `.ll`, and `lto.ll`).
- **Last checked:** 2026-11-10.

## U25. LLVM: `HasRunOnLoopT` out of an anonymous namespace

- **Library:** LLVM (`llvm/include/llvm/Transforms/Scalar/LoopPassManager.h`), llvm/llvm-project#233326 (ours).
- **Want:** `HasRunOnLoopT` in a named namespace, so `is_detected` no longer exposes a TU-local entity: a hard error
  in GCC for any module whose GMF includes `PassBuilder.h` (clang accepts it).
- **Why:** the four plain LLVM TUs (`llvm_pass_pipeline`, `llvm_pass_air`, `llvm_pass_bounds`, `llvm_pass_loop_versioning`) can become
  modules that `import llvm`.
- **Meanwhile:** they stay plain TUs that `#include` LLVM (`notes/toolchain/llvm-passes-plain-tu.md`).
- **Status (2026-11-10):** merged 2026-11-09 (moved to `namespace detail`), so in LLVM 24; not on `release/23.x`.
  A probe module with every LLVM header those TUs include and the pass code they write builds cleanly on GCC trunk
  with the patch applied, so nothing else in those headers needs the same change (MSVC, and LLVM 24's own headers,
  not tried).
- **Check:** `grep -n 'namespace detail' $(llvm-config --includedir)/llvm/Transforms/Scalar/LoopPassManager.h` above
  `HasRunOnLoopT`.
- **When it lands:** on the LLVM 24 bump, turn the four plain TUs into modules, exporting what they need from
  `headers/wrappers/llvm.ixx`.
- **Last checked:** 2026-11-10.

## U26. LLVM: the KCFI type id on Mach-O

- **Library:** LLVM (`AsmPrinter::emitKCFITypeId`, `llvm/lib/CodeGen/AsmPrinter/AsmPrinter.cpp`), llvm/llvm-project#241037
  (ours).
- **Want:** the type id emitted through `emitFunctionPrefix`, as prefix data is, so on subsections-via-symbols targets
  it gets a linker-private label and the function becomes an `.alt_entry` of it.
- **Why:** the bare word before the function belongs to the previous function's atom, so ld64 moving that atom (a
  cold function; an order file or `-dead_strip` would too) separates every typed function from its hash and the
  KCFI check at an indirect call traps (`brk #0x8229`). clang's `-fsanitize=kcfi` on Darwin has the same exposure.
  X86 overrides `emitKCFITypeId` and is not covered. Details: `notes/codegen/kcfi-cold-functions-on-macho.md`.
- **Meanwhile:** `codegen::FinishCfi` (`../sources/spp/codegen/llvm_cfi.cpp`) gives each typed definition an `i32 0`
  of prefix data on Mach-O, so LLVM's prefix path labels the atom; 4 bytes per typed function.
- **Status (2026-11-10):** unchanged on `main` `814ab0edd50c`. Reproduce: `llc -mtriple=arm64-apple-macosx -filetype=obj` on a `cold` function followed by one with `!kcfi_type`; the hash word lands inside the cold
  function's atom and the typed function has no `AltEntry` flag. A patch with the test
  `llvm/test/CodeGen/AArch64/kcfi-macho.ll` is drafted (applies cleanly to `814ab0edd50c`, `clang-format` clean); its
  checks were validated against stock `llc-23` output for the equivalent prefix-data IR, and fail on stock `llc`, but
  the patched `llc` itself has not been built. Opened as
  [llvm/llvm-project#241037](https://github.com/llvm/llvm-project/pull/241037) (2026-11-10, branch
  `users/samg101-developer/kcfi-macho-alt-entry` on the fork); it must merge before `release/24.x` branches (about
  mid-January 2027) to be in LLVM 24.
- **Check:** whether #241037 has merged and into which release; then the reproduction above shows an `ltmpN` label, the hash, and `.alt_entry`.
- **When it lands:** on the bump to the first LLVM that has it (Homebrew's, on macOS), drop the `anchor_types` prefix
  data from `FinishCfi`.
- **Last checked:** 2026-11-10.

## Resolved

- **GCC PR 127228** (libstdc++ `basic_string::_M_create` writing back the rounded capacity, which broke old-ABI inline
  callers in the prebuilt LLVM archives: padded intrinsic names, over-long triples, coroutine passes missing
  `llvm.coro.suspend`). Fixed by libstdc++ commit 8ba83d3dab2, in the local GCC since 2026-10-18. Its workarounds were
  removed then; `AssertIntrinsicNamingIsSound` stays as a start-up canary.
