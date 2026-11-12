---
name: third-party-notices
description: THIRD-PARTY-NOTICES.txt ships in every release zip; update it when spp links a dependency it did not, or a platform starts shipping one
updated: 2026-11-12
---

`THIRD-PARTY-NOTICES.txt` (repo root) goes into every release zip beside `spp` and `LICENSE.md`
(`release/collect.sh add` → `release/pack.py`). It holds, per statically linked dependency, its name, upstream, licence,
the platforms whose build contains it, and its licence text verbatim. Written 2026-11-12 for 0.7.0.

**What is in it, and why:** what `spp` links (`target_link_libraries` in `CMakeLists.txt`, `cmake/Dependencies.cmake`):
LLVM, mimalloc, unordered_dense, toml++, nlohmann/json, magic_enum, small_vector, CLI11, GenEx on every platform;
zstd on macOS and Windows (static there); zlib, libxml2, pthreads4w (static, with its `docs/NOTICE.md` and
`docs/CONTRIBUTORS.md`, which Apache-2.0 4(d) and the NOTICE itself require) on Windows. On Linux zlib and zstd are
the system's shared libraries (`check-self-contained.sh` allows exactly those), so they are not shipped there.

**Left out on purpose:** NumEx (forked from tigertv/BeeNum but rewritten; to be relicensed as its own repo, so no
BeeNum notice); ColEx (the maintainer's, no licence); range-v3 (in the manifest, linked by nothing); googletest (tests
only); libstdc++ and the MSVC runtime (the GCC Runtime Library Exception and Microsoft's redistribution terms ask for no
notice).

**Where each text comes from:** the dependency's licence file at the commit `.github/dependencies.toml` pins:
`gh api "repos/<owner>/<repo>/license?ref=<commit>"` (base64 `.content`); LLVM's `llvm/LICENSE.TXT` at the release
tag spp builds against (`[pin.llvm-win] tag`); pthreads4w keeps its files under `docs/`. The texts keep their authors'
spellings ("colaborative", the contributor "Gardian"), so the file is in `[tool.typos.files] extend-exclude`; never
spell-fix it. It is `.txt` so vale and mdformat leave it alone.

**Why:** distributing a binary under MIT, BSD, zlib or Apache-2.0 requires their notices to go with it; the LLVM
exception is for runtime pieces compiled into users' programs, not for linking LLVM's libraries into a tool.

**How to apply:** update the file when a change adds a library to what `spp` links, removes one, or changes which
platforms ship one (a library made static on Linux, say); a pin bump needs nothing unless the dependency changed its
licence. Fetch the new text as above, add a section in the same shape and a line to the contents list, and say which
platforms. The sppc runtime, linked into users' programs from SPP-STL, is a separate question with its own notices.
Related: [[released-changelogs-are-frozen]].
