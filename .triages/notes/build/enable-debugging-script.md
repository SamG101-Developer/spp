---
name: enable-debugging-script
description: ".tools/enable-debugging.sh: perf_event_paranoid=1, ptrace_scope=0, kptr_restrict=0 for perf and gdb -p; resets on WSL restart; --off restores"
updated: 2026-11-12
---

Out of the box, `perf record` refuses to run (`perf_event_paranoid` 2) and `gdb -p` cannot attach to a build that
isn't gdb's own child (`yama.ptrace_scope` 1). `.tools/enable-debugging.sh` sets both, plus `kptr_restrict` 0 so
`perf report` names kernel frames, through `sudo sysctl`. Settings last until WSL restarts, and `--off` restores the
defaults. It also warns when `perf` has no libelf, which records fine but names no symbols.

**Why:** profiling a slow `emit-object` (the full debug info and SafeStack cost, upstream-watch U29) needed `perf`, and
sampling a running build needed `gdb -p`.
**How to apply:** when a profile or an attach is refused, ask the maintainer to run `! .tools/enable-debugging.sh`.
Running a build under gdb (`gdb --args spp build ...`) needs none of this.
