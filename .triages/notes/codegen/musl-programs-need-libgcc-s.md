---
name: musl-programs-need-libgcc-s
description: "An spp program linked for x86_64-linux-musl needs libgcc_s.so.1 (__gcc_personality_v0): Alpine installs it only with libgcc"
updated: 2026-10-26
---

A program spp links for `x86_64-linux-musl` has `NEEDED libgcc_s.so.1` for `__gcc_personality_v0`, besides musl and
`libsppc.so`. glibc systems always have libgcc_s; a bare Alpine does not, and the program fails to load ("Error loading
shared library libgcc_s.so.1") until `apk add libgcc`. Checked 2026-10-26: a hello world linked with Alpine's `cc`
(via `[target.linkers]`) against sppc's musl library ran in `alpine:3.21` with only `libgcc` added.

**Why:** looks like an sppc packaging gap but is the program's own dependency, as for any C++ or Rust binary on Alpine.
**How to apply:** don't chase it in sppc; document `libgcc` for musl users, or link libgcc statically for musl if
spp wants a self-contained executable. CI's musl cross-codegen run image (`install-alpine-musl-linux.sh`) adds only
`libgcc` to Alpine, so a new runtime dependency fails there. A local musl link: put a `x86_64-linux-musl-gcc` wrapper
on PATH that runs Alpine's `cc` in podman with the project mounted at the same path (rootless: `--userns=keep-id`).
