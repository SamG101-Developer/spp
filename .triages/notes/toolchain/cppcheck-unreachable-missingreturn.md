---
name: cppcheck-unreachable-missingreturn
description: "cppcheck does not treat std::unreachable() or SPP_ATTR_NORETURN Raise as noreturn; trips missingReturn and nullPointerRedundantCheck"
---

cppcheck 2.22-dev (the blocking gate, see [[cpp-linters-local-only]]) does **not** know `std::unreachable()` is
noreturn, and it cannot see `SPP_ATTR_NORETURN` on `spp::Raise` because the attribute lives in a `.ixx` module
interface that cppcheck never parses (it only gets `-Iheaders`). So

```cpp
auto f(...) -> T* {
  if (a) { return ...; }
  if (b) { return ...; }
  Raise<E>(...);          // invisible noreturn
  std::unreachable();     // not understood
}
```

is reported as `missingReturn`, anchored at the **closing brace of the last `if`**, not the end of the function.

Fix by hoisting the raise into a leading guard clause (`if (nothing valid) { Raise... }`) and letting the last
handler fall through to an unconditional `return` - no dead code, no suppression, and the handlers stay adjacent.
`throw`, `std::abort()`, and an inline `// cppcheck-suppress missingReturn` on the
line before the reported brace all silence it too; prefer the restructure there.
Fixed this way in `CreateGenericSym` (monomorphization.cpp) on 2026-09-18.

Same blind spot gives `nullPointerRedundantCheck` for `if (p == nullptr) { Raise<E>(...); }` then `p->x` (reported at
the deref). A `--library` cfg with `<function name="spp::Raise,Raise"><noreturn>` does NOT fix it (template call
not matched). The maintainer chose an inline `// cppcheck-suppress nullPointerRedundantCheck` above the deref (cfg.cpp, 2026-10-23)
over `SPP_ASSERT(p != nullptr)` after the raise (also works, but is dead code for the linter).

Most other `std::unreachable()` sites in the tree are not flagged because they sit after a `switch` with a `default`,
or in files cppcheck bails out of.

Reproduce one file without the full run:
`cppcheck --enable=warning,performance,portability --inline-suppr --suppressions-list=.cppcheck-suppressions --check-level=exhaustive --std=c++26 --language=c++ --quiet -Iheaders <file>`
