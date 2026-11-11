---
name: instantiated-from-error-site
description: errors::InstantiatedFrom is the outermost site an instantiation was asked for; errors raised inside one get an "Instantiated here" block
updated: 2026-11-05
---

`errors::InstantiatedFrom()` (in `semantic_error.ixx`) is the outermost site the instantiation being made or analysed
was asked for. It is set by an `InstantiatedFromGuard`, which marks a site only when none is marked yet, so the
outermost wins:
- `FindOrMakeClsInstance` marks the type naming the instance (`type_part`);
- `AnalysePendingGnSubstitutions` marks `sub.OutermostRequest()->RequiredAt`, the outermost call, for a function body.

`SemanticErrorBuilder::Raise` adds an "Instantiated here" context block before the footer when the site is in a file
(`ErrorFormatter::Owning`, one formatter per file) and none of the error's blocks is in that file. The field borrow
check (`ClassAttributeAst`, `RequireFieldNotBorrowed`) also uses the site: a field borrowed only by substitution is
reported at the first borrow written in the site.

**Why:** an error inside std's `Some[T]`, caused by a user's `Opt[&Str]`, otherwise quotes only std (open-bugs #50).

**How to apply:**
- A new instance maker or delayed-analysis path should take an `InstantiatedFromGuard` too.
- Test macros require every "Error in" block to be in the test's `main.spp`, while context blocks may point anywhere.
  So moving a block's role (error versus context) changes whether a test passes.
- The linear-value error uses `OutermostRequest` directly, to make the call the error block.
- A site must outlive the error that reads it. A class site is the guard's own frame's. `RequiredAt` is owned by the
  substitution (a `Shared<Ast const>`), because the requiring call can be a temporary. It is a one-node stand-in carrying
  the call's span (`WithSourceSpanAt`), not a copy: a full copy of a call at the end of a `.get().get()` chain holds
  the whole chain, and cost `generic-depth` 0.93M AST nodes (+5.7%, 2026-11-04). Only a call with no span (generated)
  is copied whole, to be shown as text.
- Only class instances and function bodies mark a site. Function signature substitution (`FindOrMakeFnInstance`)
  has no call AST to hand.
