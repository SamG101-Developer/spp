---
name: resume-ends-mut-yields
description: "A second g.res() ends every &mut the generator yielded before (E5/E8 on later use); how it's tracked, and the variant-typed holder trap"
updated: 2026-09-28
---

Since 2026-09-28, `PostfixExpressionOperatorKeywordResAst::Stage8_CheckMemory` works like this:
- **Recording:** when a resume of a named generator yields `&mut` and is bound (`meta->AssignmentTarget`), the target's
  name is recorded in `MemoryState::AstYieldedMutBorrowHolders` on the generator's symbol. `MemoryState` is the part
  snapshots copy, so it merges across branches and loops.
- **Invalidating:** the next resume calls `MovedBy(res)` on each holder. A later use is then E5, or E8 when the resume
  was in a branch or loop that might not run.
- **Only `&mut` yields** are tracked. `&` yields coexist; `test_valid_memory_use_ref_borrow_after_conflicting_ref_borrow_created_for_resume` pins that.

**Trap:** a bound resume result is typed `&mut T or GenDone`, not `&mut T`. So "is this holder still a &mut" has to look
through variant members (`HoldsMutBorrow`). A top-level `Conv` check silently disabled the whole feature. That guard
exists so a name rebound since the resume (`let x = 5`) isn't ended.

**Not covered:** a borrow copied on out of the holder (`e2 = e1` in a case branch), and unnamed generators.
Related: [[generator-gen-done-sentinel]], [[spp-linear-types]].
