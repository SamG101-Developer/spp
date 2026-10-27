---
name: qemu-getsockopt-timeout-empty
description: "qemu-user returns SO_RCVTIMEO/SO_SNDTIMEO from getsockopt as success with length 0: std's timeout read-back tests fail under every qemu target"
updated: 2026-10-26
---

qemu's `linux-user/syscall.c` `do_getsockopt`, timeout branch (`get_timeout:`), sets `len = 0` when `optval_addr` is
non-NULL (the condition is inverted; its comment says the NULL case). So under qemu-user any `getsockopt` of
`SO_RCVTIMEO`/`SO_SNDTIMEO` succeeds, copies nothing and reports length 0. Seen with 11.1.2 on aarch64 and s390x;
still in qemu master on 2026-10-26. Natively the same C probe reads back 252 ms for 250 ms set.

**Why:** `spp test --all-libs --target <t> --runner "qemu-<arch> -L <sysroot>"` fails exactly
`std::tst::net::{tcp,udp}::test_a_{read,write}_timeout_reads_back_as_it_was_set` (550/554 on aarch64) - not an
sppc or std bug.
Which targets show it varies: qemu copies nothing, so the buffer keeps whatever was there, and where sppc's
`getsockopt` reuses the stack slot the `setsockopt` just filled (aarch64_be), the value set is read back and the
tests pass by luck. sppc does not check the returned length, which a real kernel always fills.

**How to apply:** CI's qemu is our fork's release (`[pin.qemu-user]`, `SamG101-Developer/qemu`), which carries the
fix, so these four pass there; if they fail again under qemu, the pin has gone back to an unfixed qemu. Under any
other qemu, check them natively. Tracked as U17 in `upstream-watch.md`; delete this note once a qemu release has
the fix.
