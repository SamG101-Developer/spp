---
name: ide-recovery-sectioning
description: "How the IDE (--message-format=json) error recovery works: per-member sectioning in stages 6-9, the override pre-pass, the walk rewind, E45 suppression for failed blocks, and what stays unrecoverable"
updated: 2026-10-04
---

Recovery only runs with the diagnostic sink on (`CompileReportingJson`, i.e. the IDEA plugin). Normal compiles still stop at the first error. State as of 2026-10-04:

- **Unit = module member** (a whole `sup` block, not one method). `RunMember` / `CheckMember` in
  `module_implementation_ast.cpp` catch, `diagnostic_sink::Report` (poisons the member by AST pointer), and later stages
  skip poisoned members. Stages 6, 7, 8, 9 are sectioned; stages 1-5.5 are NOT (declarations + sup graph) - an error
  there still drops the whole index.
- **Override pre-pass**: `SupPrototypeExtensionAst::CheckExtensionMembers` (E62 + non-virtual override + type/cmp
  member checks) runs for every block at the start of stage 6, before any member's stage 6. Before this, module order
  decided whether the cause (E62) or its consequence (E45 at a use of the now-abstract type) was reported.
- **Walk rewind**: `RunMember` snapshots `CurrentScope` + iterator and `Reset`s to it before `SkipPastScope`. An error
  raised after a member's last nested scope was walked (E94 at a `ret case ...`) left the iterator on the final scope
  with `CurrentScope` on a parent, so the forward-only skip ran off the end and the next member segfaulted.
- **Scope ownership**: `Ast::Stage2_GenTopLvlScopes` sets `_Scope` to the *declaring* scope, so a scope-less member
  (a `use` statement) "owned" the module scope and a failure skipped the rest of the module. `RunMember` only skips a
  scope that is not the walk's own or an ancestor (`OwnsScope`), and `UseStatementAst` takes its conversion's scope.
- **Index-only analysis must never raise or move the walk**: `TypeStatementAst::_IndexWrittenTarget` analyses the
  written `type`/`use` target (only while indexing) inside try/catch, and restores `CurrentScope` + iterator -
  resolving a namespaced name moves the walk. A bare generic (`use std::option::Opt`) throws and gets no record.
- **E45 suppression**: `type_members::IsLeftUnimplementedByAnError` - in sink mode an abstract method doesn't count if
  a poisoned extension block on the type declares a method of that name. Instance sup scopes share the template
  block's AST (`MakeSupInstance` copies the scope, not the AST), so the pointer check works for generics.
  `overload_resolution.cpp:621` is a second abstractness check that was NOT given this treatment.

**Why:** a stale std (`spp-stl` with the `RawBuf` `or None` returns) gave the plugin one misleading E45 and an empty
index; now it gets both real E62s, the unrelated E94s, and a full index.

**How to apply:** tests live in `tests/compiler/test_msc_json_diagnostics.cpp` (`RecoveringCompile` fixture). Any new
stage-level recovery needs a crash probe in JSON mode, not just a pass. Related: [[sup-attach-fixed-point]],
[[spp-meta-flags-are-per-stage]].
