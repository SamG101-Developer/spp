---
name: error-block-at-the-users-code
description: "An error's red block (AddErr) marks the code the user must change; declarations it refers to, even in std, are context (AddCtxForErr)"
updated: 2026-11-07
---

`tests/compiler/test_macros.hpp` checks every should-fail test's output: the "Error in file" block must be in the
test's own `src/main.spp`, while "Context from file" blocks may be anywhere (std included), and no block may be
"In generated code". So an error's `AddErr` goes on the use the user has to fix, and the declaration it is about goes
in `AddCtxForErr`. E72 had them reversed until 2026-11-07: it marked `cls Pass[T]` in `result.spp` as the error and
gave the type's `FqName()` (a generated AST, no position) as context, so `ret Pass()` never appeared at all. It had
no test, which is how it went unnoticed.

**Why:** the red block is where the user looks first; a std line there tells them nothing they can change.

**How to apply:** give an error the AST as written at the use (`*this`, the call's `site.Written`), never a name the
compiler builds (`FqName()`, an `InferType` result). Add a `SPP_TEST_SHOULD_FAIL_SEMANTIC_AT` test with the expected
underline for each new error, so its location is checked, not only its type.
