---
name: spp-run-binary-memory-cap
description: "Why running the compiled S++ binary used to kill WSL, and the ulimit guard that stops it"
updated: 2026-09-12
---

Repeated "WSL crashed" events during 2026-08-31 were **not** the harness: the kernel OOM-killer was
killing the compiled S++ test binary (`project/out/project`, `anon-rss:15549312kB`) and taking the VM
with it. Confirmed via `dmesg | grep -i oom-kill`.

Root cause was an unbounded allocation loop, not a leak — see [[spp-full-width-shift-is-poison]].

`GlobalAlloc::allocate` already maps a null `malloc` to `AllocOomErr`, but **on Linux that branch is
unreachable by default**: overcommit hands out address space, `malloc` succeeds, and the process dies
when it touches the pages. The allocator cannot defend the machine on its own.

Fix: `WithMemoryLimit()` in `sources/spp/cli.cpp` prefixes `ulimit -v <kb> && ` onto the `std::system`
command in both `HandleRun` and `HandleTest`. Default 4 GiB, overridable with `SPP_MEMORY_LIMIT_KB`
(0 disables). This makes `malloc` genuinely return null at the cap, so `AllocOomErr` fires as designed
and a runaway reports itself instead of killing the box. Verified not to affect a normal run.

**Harness trap (2026-09-13):** never launch `spp test`/`spp run` from a shell that already ran `ulimit -v` (as the
gtest runs do). bash's `ulimit -v` sets the hard limit too, so the per-test `ulimit -v <kb>` cannot raise it: every
test prints `ulimit: error setting limit (Operation not permitted)`, "A test did not finish", and the suite reports
0/516 - which looks like a compiler regression and is not. Start `spp test` before the `ulimit`, or from a fresh shell,
or set `SPP_MEMORY_LIMIT_KB=0` for that one run.

**Do not remove this to "speed something up".** Without it, any non-terminating allocation loop in
`.spp` code is a machine-killer, and those are easy to write — see `no-benchmark-runs` for the
related rule about not looping the binary.
