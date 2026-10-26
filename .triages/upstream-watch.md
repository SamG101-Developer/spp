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
| U1  | LLVM             | New pass manager codegen pipeline on every target               | 2026-10-24   |
| U2  | GCC              | `extern "C++"` forward declarations across modules              | 2026-09-13   |
| U3  | GCC              | `lookup_mark` ICE on unqualified calls and cross-module friends | 2026-10-08   |
| U4  | GCC              | "Bad file data" reading a compiled module cluster               | 2026-10-08   |
| U5  | clang            | Implicit destructor ICE in an importer (`GetVTTParameter`)      | 2026-10-14   |
| U6  | MSVC STL         | `std::string + std::string_view` (P2591)                        | 2026-09-11   |
| U7  | MSVC             | Inline bodies from a GMF instantiated in importers              | 2026-09-11   |
| U8  | CMake            | `vs_link_exe` treating a leading `@` as a response file         | 2026-09      |
| U9  | ccache-action    | `append-timestamp: false`                                       | 2026-10-21   |
| U10 | Ubuntu toolchain | A current GCC release for the runner images                     | 2026-10-09   |
| U11 | ccache, sccache  | Caching C++20 module compiles from GCC                          | 2026-10-24   |
| U12 | sccache          | Caching C++20 module compiles from cl.exe                       | 2026-10-24   |
| U13 | LLVM             | PC-relative EH pointers on m68k under PIC                       | 2026-10-24   |
| U14 | CMake            | `import std` with clang-cl and the MSVC STL (CMake 4.5)         | 2026-10-25   |
| U15 | LLVM             | SafeStack checking a canary through the target's own route      | 2026-10-25   |
| U16 | LLVM             | AArch64 SLS indirect-call thunks named for Mach-O               | 2026-10-26   |

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
  - AMDGPU, BPF, Lanai and MSP430.

  None on trunk: AArch64, ARM, PowerPC, SystemZ, MIPS, SPARC, LoongArch, M68k, Hexagon. Even on X86 it is experimental
  (`llc -enable-new-pm`). In the installed LLVM 23, only X86, AMDGPU, BPF, Lanai and MSP430 have it.

- **Check:** for each target in the cross-codegen list, does
  `llvm/lib/Target/<T>/<T>CodeGenPassBuilder.cpp` exist, or does `<T>TargetMachine.cpp` override
  `buildCodeGenPipeline`? Does `llvm/docs/NewPassManager.rst` still say codegen uses the legacy pass manager? Locally:
  `nm -C libLLVM<T>CodeGen.a | grep "TargetMachine::buildCodeGenPipeline"`.

- **When it lands:** build object emission with `buildCodeGenPipeline` on a `ModulePassManager` (with
  `CGPassBuilderOption`), check every cross target in CI, then drop the legacy path.

- **Last checked:** 2026-10-24 (trunk, LLVM 23 archives, docs, Discourse).

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
- **Check:** remove one of the redeclarations that exist only for bug 1 (e.g. `GenericArgumentAst` in
  `literal_ast.ixx`) and build with the current GCC, using the project's `-Wall -Wextra -Werror`; grep for
  `warning` as well as `error`. A standalone repro would need writing again.
- **When it lands:** turn the AST forward declarations into non-exported `extern "C++"` purview declarations, and drop
  the redeclarations that exist only for bug 1.
- **Last checked:** 2026-09-13.

## U3. GCC: `lookup_mark` ICE

- **Library:** GCC (`cp/tree.cc:2637`, `lookup_mark`).
- **Want:** no ICE ("internal compiler error: in lookup_mark") for (a) an unqualified call to a free function of the
  same module from a member function body, and (b) a friend function overloaded across modules.
- **Why:** calls could be written unqualified, and an overloaded function could be befriended.
- **Meanwhile:** such calls are fully qualified (`spp::analyse::scopes::PrecompiledTemplate` in `TypeRef::IsA`,
  `BindByName`/`ParamsOfGroup` in `TypeSymbol::AliasTargetId`, both in `symbols.cpp`); the `TypeKey` caches are public
  `_`-prefixed fields rather than befriended. If a tidy drops one of those qualifications and the build ICEs in
  `lookup_mark`, restore it.
- **Status:** not reduced to a standalone repro or reported, as far as the notes say.
- **Check:** unqualify one of the calls above and build.
- **When it lands:** calls can drop their qualification where a style choice wants it.
- **Last checked:** 2026-10-08.

## U4. GCC: "Bad file data" reading a compiled module cluster

- **Library:** GCC (C++ modules, reading `.gcm`).
- **Want:** no deterministic "failed to read compiled module cluster N: Bad file data" (and "failed to load pendings for
  'std::allocator'") for certain import graphs.
- **Why:** an interface could import what it needs rather than forward-declare around it.
- **Meanwhile:** a `use()` forward declaration instead of an interface import where only a pointer or reference is
  needed (`instance_key.ixx` and `CompNode`, 2026-10-02); AST-needing functions moved into a second TU
  (`substitution.cpp`, 2026-10-08).
- **Status:** not reduced or reported, as far as the notes say.
- **Check:** add `import spp.analyse.scopes.comp_key;` back to `instance_key.ixx` and build.
- **When it lands:** the workarounds above can go back to plain imports.
- **Last checked:** 2026-10-08.

## U5. clang: implicit destructor ICE in an importer

- **Library:** clang (codegen of an implicit destructor for a type from another module; stack
  `destroyCXXObject` -> `GetVTTParameter` -> `castFromDeclContext`).
- **Want:** an importer emitting an exported aggregate's implicit destructor does not segfault. Seen with clang 23.1.2
  on `PlaceAt` (`regions.ixx`, an aggregate holding an `Opt<Place>`) once `Place` gained a defaulted `<=>`.
- **Why:** special members could be left implicit.
- **Meanwhile:** `PlaceAt` declares all its special members and defines them `= default` in `regions.cpp`, inside
  `SPP_MOD_BEGIN`/`SPP_MOD_END`. The same stack on another `SPP_EXP_CLS` type gets the same fix.
- **Status:** a modules merge bug; GCC is fine. Not recorded as reported.
- **Check:** with the clang tree (see the local clang coverage build), drop `PlaceAt`'s out-of-line special members and
  build `symbols.cpp`.
- **When it lands:** remove the out-of-line special members.
- **Last checked:** 2026-10-14.

## U6. MSVC STL: `std::string + std::string_view`

- **Library:** Microsoft STL (P2591, C++26).
- **Want:** `operator+` for `std::string` and `std::string_view`, as libstdc++ already has.
- **Why:** a `Str + StrView` chain builds on Windows without converting first.
- **Meanwhile:** convert the view first with `AsStr(StrView) -> Str` (in `semantic_error.cpp`; `Str(x)` would trip GCC's
  `-Wuseless-cast`), and keep macros that expand to a `+` chain (`INLINE_INFO` and friends) parenthesised.
- **Status:** not implemented in the MSVC STL as of the note.
- **Check:** the MSVC STL changelog or status page for P2591, or a one-line test on the Windows runner.
- **When it lands:** `AsStr` at those splices can go (keep the macro parentheses).
- **Last checked:** 2026-09-11.

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
- **Status:** still present in CMake 4.4.0.
- **Check:** `cmcmd::VisualStudioLink` in the current CMake source, or the Windows build with the override removed.
- **When it lands:** remove the archive directory override (both places).
- **Last checked:** 2026-09 (CMake 4.4.0).

## U9. ccache-action: `append-timestamp: false`

- **Library:** `hendrikmuhs/ccache-action` (`restore.ts`).
- **Want:** `append-timestamp: false` honoured. In v1.2.24 the input is tested as a string, so `"false"` is truthy and a
  trailing `-` is appended to the key anyway.
- **Why:** cache keys without a timestamp.
- **Meanwhile:** the default (timestamped keys) and no sha in the key; `prune-caches.sh` groups `ccache-`/`sccache-`
  keys with `:3`, as the ISO date splits into three dash segments.
- **Status:** broken in v1.2.24.
- **Check:** on each ccache-action bump, read `restore.ts` for the string test.
- **When it lands:** switching to `append-timestamp: false` is possible; keep the prune segment count in step with the
  key shape.
- **Last checked:** 2026-10-21.

## U10. Ubuntu toolchain: a current GCC for the runner images

- **Library:** `ppa:ubuntu-toolchain-r/test`, and Ubuntu's own packages.
- **Want:** a GCC 16 release (with the fix for PR125768, the imported-incomplete-type modules ICE in `symbols.cpp`)
  for the Ubuntu versions the GitHub runner images use.
- **Why:** CI could take GCC from apt rather than Homebrew.
- **Meanwhile:** `../.github/scripts/setup/install-gcc-linux.sh` installs GCC from Homebrew, pinned by `[pin.gcc]`
  in `../.github/dependencies.toml` (matched against `g++ -dumpfullversion`). It installs Homebrew's binutils too (its GCC
  emits `.base64`, which the runner's binutils 2.42 rejects) and strips Homebrew's specs-file loader and rpath
  additions. The clang jobs still take libstdc++ from the PPA.
- **Status:** the PPA's newest gcc-16 for noble is a 2026-03-15 trunk snapshot, before the PR125768 fix. GCC 16.2
  packaging exists only in Ubuntu 26.10 and Debian sid, neither of which has a runner image.
- **Check:** the PPA's gcc-16 package date for the runner's Ubuntu release, and whether GitHub ships an image on a
  release with GCC 16.2 or later.
- **When it lands:** go back to apt for GCC; the Homebrew binutils and specs workarounds go with it.
- **Last checked:** 2026-10-09.

## U11. ccache, sccache: caching GCC's C++20 module compiles

- **Library:** ccache (#1252, draft PR #1523) and sccache (#2630).
- **Want:** GCC compiles with `-fmodules-ts` cached, with the imported and produced `.gcm` files hashed and restored.
- **Why:** the GCC jobs build every TU with modules, so the compiler cache does nothing for them.
- **Meanwhile:** none needed for correctness. ccache 4.14.2 counts these compiles as unsupported and sccache 0.19.0
  refuses them, so they always compile fresh. The launcher stays on for GCC so a release that adds support takes effect.
- **Status:** ccache#1523 had GCC 14 caching working but stalled in 2026-02. Its author says the compilers need upstream
  patches first: their P1689 output leaves too much out. sccache#2630 has no PR. sccache#2516's author calls GCC's
  module handling "a mess" that needs an entirely different approach.
- **Check:** the ccache and sccache release notes for GCC module support, and ccache#1523 and sccache#2630. Confirm on
  CI: the ccache stats on a second GCC build should show hits rather than `unsupported_compiler_option`.
- **When it lands:** bump `[pin.ccache]`/`[pin.sccache]`. Before trusting it, check that a change to a module interface
  alone makes its importers miss: export `f(int)`, compile an importer through the cache, change it to
  `f(int, int = 0)`, rebuild the module and the importer, and `nm` must show the new mangling (`_ZW1m1fii`).
- **Last checked:** 2026-10-24 (ccache 4.14.2, sccache 0.19.0).

## U12. sccache: caching cl.exe's C++20 module compiles

- **Library:** sccache (#2629, PR #2934).
- **Want:** cl.exe module compiles (`/interface`, `/ifcOutput`, `/reference`, `@<file>.modmap`) cached, with the
  imported IFCs hashed.
- **Why:** the Windows MSVC job gets a 0% hit rate. The clang-cl job is already cached (sccache #2876, in 0.19.0).
- **Meanwhile:** none needed for correctness. sccache 0.19.0 refuses every cl.exe module flag, so these compile fresh.
  Do not move Windows to ccache: before ccache PR #1837 it caches MSVC module compiles and can return stale objects.
- **Status:** PR #2934 opened 2026-10-23, unreviewed. `/headerUnit` would stay uncacheable.
- **Check:** the sccache release notes for #2934, then the MSVC job's sccache stats on a second build.
- **When it lands:** bump `[pin.sccache]` and confirm that a module-interface-only change makes importers miss.
- **Last checked:** 2026-10-24 (sccache 0.19.0).

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
  2026-10-24.
- **Check:** whether #235347 has merged and into which release; then
  `grep -n "case Triple::m68k" llvm/lib/CodeGen/TargetLoweringObjectFileImpl.cpp` in the installed LLVM's
  release branch, or emit m68k with the key on and `readelf -r spp.o` for `R_68K_32` in `.rela.eh_frame`.
- **When it lands:** once spp builds against an LLVM with the fix, delete `TargetHasPcRelEhPointers` and its refusal,
  drop `ffi_unwind_guard` from the m68k entry's `off` in `_cross_codegen.yaml`, and decide whether the key stays (S20.1
  had it keyless: it protects a language guarantee).
- **Last checked:** 2026-10-24.

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
  so 4.5.0 is expected around mid-November 2026 (an estimate, not an announced date). Whether clang 23 then compiles
  the MSVC STL's `std.ixx` cleanly is untested.
- **Check:** `gh api repos/Kitware/CMake/releases --jq '.[0:3][].tag_name'` for a final 4.5.0; the top of
  `Clang-CXX-CXXImportStd.cmake` at that tag should include `Compiler/MSVC-CXX-CXXImportStd` for the MSVC frontend.
- **When it lands:** bump `[pin.cmake]` in `../.github/dependencies.toml` to 4.5.0 (the sibling libraries are built with
  the same CMake), run the clang-cl lane, and once it is green drop `advisory: true` and the "(advisory)" from its
  name in `pr.yaml`, and consider adding it to `ci-success`. Also recheck U8 on the new CMake.
- **Last checked:** 2026-10-25 (CMake tags and source, cmake-cxxmodules(7) for 4.5.0-rc1).
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
- **Status (2026-10-25):** unchanged on trunk (`09ec836bbe83`); no upstream issue found.
- **Check:** in `SafeStack.cpp`, whether `checkStackGuard` still errors when `STACKPROTECTOR_CHECK_FAIL` is
  unsupported; or `llc -mtriple=x86_64-pc-windows-msvc` on a function with `sspstrong safestack`.
- **When it lands:** delete `HasSafeStackCanary`/`TargetHasSafeStackCanary`, the refusal and its Todo, the two
  `TestTargetSupport` canary tests, and `memory.stack.split` from the Windows lanes' `off` in `pr.yaml` and
  `post_merge.yaml`.
- **Last checked:** 2026-10-25.

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
- **Status (2026-10-26):** unchanged on trunk (`09ec836bbe83`). Reproduce: `llc -mtriple=aarch64-apple-macos -filetype=obj` on a function doing `call void %p()` with `"target-features"="+harden-sls-blr,+harden-sls-nocomdat"`;
  `llvm-nm` shows `t ___llvm_slsblr_thunk_x0` and `U __llvm_slsblr_thunk_x0`. No upstream issue filed yet.
- **Check:** the same `llc` + `llvm-nm`: one defined name, no `U` thunk.
- **When it lands:** restore the AArch64 Mach-O case in `TargetHasSlsHardening`, with `+harden-sls-nocomdat` added to
  its features on Mach-O again (Mach-O has no COMDATs), and drop `off: sls` from the macOS test calls.
- **Last checked:** 2026-10-26.

## Resolved

- **GCC PR 127228** (libstdc++ `basic_string::_M_create` writing back the rounded capacity, which broke old-ABI inline
  callers in the prebuilt LLVM archives: padded intrinsic names, over-long triples, coroutine passes missing
  `llvm.coro.suspend`). Fixed by libstdc++ commit 8ba83d3dab2, in the local GCC since 2026-10-18. Its workarounds were
  removed then; `AssertIntrinsicNamingIsSound` stays as a start-up canary.
