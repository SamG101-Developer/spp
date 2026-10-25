---
name: macos-libcxxabi-link
description: "macOS links the pinned LLVM release's libc++ and libc++abi statically via LDFLAGS; as dylibs, dyld bound the system libc++ at run time and crashed in static init"
updated: 2026-09-11
---

macOS CI builds with the LLVM release tarball unpacked at `$SPP_LLVM_MAC_PREFIX`
(`.github/scripts/setup/install-llvm-macos.sh`). Since 2026-09-11
`install-clang-macos.sh` sets `LDFLAGS=-nostdlib++ <prefix>/lib/libc++.a
<prefix>/lib/libc++abi.a`, found with `find` and a hard error if missing. The
old `if (APPLE) target_link_libraries(spp_lib PRIVATE c++abi)` in CMakeLists.txt
is gone. Don't bring it back: a stray `-lc++abi` would pull a second, dynamic
copy of the ABI library into the link.

**Why static (the runtime half):** linked as `@rpath/libc++.1.dylib`,
`spp_tests` aborted before main. `locale::~locale` in gtest's static init
reported "pointer being freed was not allocated". The libc++ frames were at
`0x1969…`, which is the dyld shared cache, so the *system* `/usr/lib` libc++ had
been bound, and it is older than the LLVM 23 headers. A disk-loaded dylib never
maps in that range, so frame addresses alone tell you which copy ran. The
released macOS `spp` had the same dependency, with an rpath into the runner's
`$HOME`, so on a user's machine it could only ever get the system copy.

**Link-time half (still true):** the release's libc++ does not carry libc++abi
(Apple's `libc++.tbd` re-exports it; this one doesn't), so dropping libc++abi.a
gives undefined `__cxa_*`, `___gxx_personality_v0`, `operator new`,
`std::terminate`, and the runtime_error/invalid_argument vtables. Only
ABI-owned symbols go undefined, which makes it look like no C++ runtime was
linked at all.

**Static did NOT fix the abort (2026-09-11, commit 858e42eb):** with libc++.a
linked, spp_tests still aborts in `--gtest_list_tests`, now *silently* (nothing
on stderr, so not a malloc/libc++/terminate message). Ruled out: stale
`~/.local` gtest (rebuilt 09-09, after the LLVM 23 switch), init_priority on
Mach-O (clang accepts it). Root cause still unknown; keep `DISCOVERY_MODE
PRE_TEST` and the lldb block in run-tests.sh until a backtrace names it.

**How to apply:** if a macOS binary's `otool -L` shows `@rpath/libc++`,
`libc++abi` or `libunwind` again, the static flags were lost (cached build
tree, a new driver default, `-L<prefix>/lib` reintroduced). Look there, not at a
code change. Related: [[sppc-sibling-repo]].

**Backtrace found (2026-10-25):** the abort is no longer silent. `locale::~locale` in the *system* `libc++.1.dylib`
(`0x18a…`, shared cache) is called from `spp_tests`'s `FlagToEnvVar` even though `otool -L` lists no libc++ dylib.
Likely cause: dyld coalesces exported weak libc++ symbols with the system copy that every process loads. Tried:
`-Wl,-load_hidden,` on both archives in `install-clang-macos.sh`, plus `nm -m` and `DYLD_PRINT_BINDINGS` lines in
`gtest.sh`'s macOS crash block. If it still fails, read those two outputs before trying anything else.
