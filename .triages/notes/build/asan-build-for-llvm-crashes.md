---
name: asan-build-for-llvm-crashes
description: "gdb cannot unwind through the prebuilt LLVM; use the ASAN build to locate crashes in codegen"
updated: 2026-08-16
---

The system LLVM (llvm-23) is linked without unwind info, so a crash inside it gives gdb a one-frame backtrace (`#0 llvm::…`, `#1 0x0` and nothing else). Do not try to recover the caller by reading the stack — heuristics there produce `.rodata` pointers, not return addresses.

Configure and use the ASAN build instead:

```
cmake -S . -B <debug-asan build dir> -DSPP_SANITIZER=address
ASAN_OPTIONS=use_sigaltstack=1:fast_unwind_on_fatal=1 ./<debug-asan build dir>/spp
```

**Why:** It reports the full symbolised stack, which is what finally located a codegen crash after a long stretch of blind printf probing. Note `-fno-omit-frame-pointer` comes with the sanitizer flags, which is what makes our own frames unwindable.

**How to apply:** Reach for it as soon as a codegen crash trace bottoms out in LLVM, not as a last resort. Two readings that matter: `si_addr == $rsp - 8` means a genuine stack overflow, and if the reported frames are shallow and non-repeating, the recursion is inside LLVM (typically a cyclic or invalid type reaching `DataLayout`) rather than in our code. See [[build-workflow]].
