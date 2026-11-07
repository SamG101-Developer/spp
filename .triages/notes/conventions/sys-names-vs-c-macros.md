---
name: sys-names-vs-c-macros
description: "A sys:: name that is a C macro breaks in any file that also includes the C header, gtest included; #undef it there"
updated: 2026-10-30
---

`sys` exports POSIX names that are macros in the C headers (`WIFEXITED`, `errno`, `stdout`, `O_RDWR`, ...), with `sys.ixx`
capturing and `#undef`ing them in its own global module fragment. Macros do not cross `import`, so a plain module file is
fine, but a file that also `#include`s the header sees the macro, and `sys::WIFEXITED(x)` expands to nonsense
("expected unqualified-id before '('"). `test_macros.hpp` pulls in gtest, which pulls in `<stdlib.h>`.

**Why:** this cost a failed `spp_tests` build on `test_msc_sys.cpp`; `cli.cpp` used the same names without trouble.

**How to apply:** in such a file, `#undef` the names after the includes, as `tests/compiler/test_msc_sys.cpp` does. Do not
rename the export. When a suite's count looks low after a failed build, the run used the old `spp_tests`.
