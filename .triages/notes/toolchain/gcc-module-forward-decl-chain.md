---
name: gcc-module-forward-decl-chain
description: "Every AST forward decl is SPP_EXP_CLS in the module purview (SPP_FWD_CLS was deleted) because GCC mishandles non-exported and GMF extern \\\"C++\\\" decls across modules; plus the redeclare-every-intermediate rule"
updated: 2026-09-13
---

**Current policy (2026-09-11):** every forward decl in the AST headers is `SPP_EXP_CLS` (`export extern "C++"`) inside the module purview. The per-file form is `SPP_AST_COMMON_FWD_DECL(Self) { SPP_EXP_CLS struct X; }`, placed after the imports; the macro also declares `ScopeManager`, `CompilerMetaData` and the exported self decl. `SPP_FWD_CLS` was deleted from `macros.hpp`. The global module fragment holds `#include`s only. Keep it this way until GCC is fixed. At that point the goal is non-exported (`extern "C++"`) purview decls - The maintainer doesn't want these names exported.

GCC 17 trunk gets `extern "C++"` forward decls across modules wrong in three ways. The first two were reproduced with standalone modules in the session scratchpad, and clang 23 accepts all three:

1. **Merges only from a direct import (2026-09-02).** An intermediate module that doesn't redeclare the type gives "marked 'override', but does not override" plus a `-Woverloaded-virtual` "was hidden" note. Fix: redeclare the type in **every** `.ixx` on the import chain between the base and each override - for `ExpressionAst` that meant `primary_expression_ast`, `literal_ast` and `array_literal_ast`. This is what the `// TODO: GCC BUG REQUIRES THIS` decls were for.
2. **A non-exported purview decl can't be redeclared by an importer.** Module `a` has `extern "C++" struct L;` in its purview; module `b` imports `a` and has the same line. GCC reports "conflicting declaration", drops `b`'s decl, and then says "'L' was not declared". What matters is **visibility, not whether the decl is exported**. The failing case is an importer redeclaring a type whose **definition** is reachable but not visible - for example `sup_implementation_ast` redeclared `Ast` while reaching `ast.ixx` only through `inner_scope_ast`'s plain `import`. Redeclaring when only a decl is reachable is fine, which is why every AST's self decl coexists with `ast_kind_range`'s exported decls. Fix: import the defining module directly; it's already in the closure, so there's no cycle. Bug 2 is a **GCC 16 regression**: 14.2, 15.1 and 15.2 accept it; 16.1 and 17 trunk reject it. The Bugzilla-ready report is in `docs/bug2/` and `docs/bug2.tar.gz`, alongside bug1's; GCC Bugzilla blocks automated fetches (Anubis), so it couldn't be checked for duplicates.
3. **GMF and purview decls of one type mixed along a chain break overrides.** `kr` declares `D` in its purview, `base` imports `kr` and declares `D` in its GMF, and `d` redeclares `D` and overrides a virtual taking it. That gives "does not override".

**Why the GMF route was abandoned:** moving the decls into the GMF fixed bug 2 but caused bug 3 wherever another module (`ast_kind_range`, the mixins) kept purview decls of the same type. It also needed `-Wno-global-module` (`-Werror` rejects declarations written straight into the GMF), and it hid the decls from `.cpp` implementation units, which see the interface's purview and imports but never its GMF. Using the same kind of decl everywhere, exported and in the purview, avoids all three problems.

**Form since 2026-09-13:** the headers use `SPP_AST_COMMON_FWD_DECL(Self);` followed by one `use(ns, struct X);` per decl (the `use` macro emits the exported purview decl plus a `using namespace`). The conversion broke the build twice, both by dropping decls that only existed for these bugs: `ast.ixx` lost its own self decl (every importer then saw `Ast` as incomplete, "expected class-name" at `: Ast`), and `literal_ast.ixx` / `primary_expression_ast.ixx` lost `GenericArgumentAst` / `ExpressionAst` (bug 1: `TupleLiteralAst::ReadExpr` "does not override"). A decl that looks unused in a header may be a bug workaround - diff the decl set against HEAD before removing any.

**How to apply:**
- New forward decls go in the purview as `SPP_EXP_CLS`; never `extern "C++"` alone, and never in the GMF.
- Don't flip them to non-exported to "tidy up" while GCC still has these bugs.
- When testing a GCC fix, repro with the project's `-Wall -Wextra -Werror` and grep for `warning` as well as `error|note`. I missed the GMF warning at first by filtering it out.
- 2026-10-02: adding `import spp.analyse.scopes.comp_key;` to instance_key.ixx made every importer fail with
  "failed to read compiled module cluster N: Bad file data" on instance_key (deterministic, not a stale gcm). A
  `use(spp::analyse::scopes, struct CompNode);` forward decl instead of the import fixed it. Prefer a `use()` forward
  decl over a new interface import when only a pointer/reference is needed.
- 2026-10-02: an unqualified call to a free function of the same module from a member function body
  (`BindByName(ParamsOfGroup(..))` inside `TypeSymbol::AliasTargetId`, symbols.cpp) ICEd GCC 17 ("internal compiler
  error: in lookup_mark, at cp/tree.cc:2637"). Fully qualifying both calls (`spp::analyse::scopes::...`) fixed it.

Related: `expression-generic-substitution`, [[msvc-requires-clause-token-match]].
- 2026-10-08: "fatal error: failed to load pendings for 'std::allocator'" plus "failed to read compiled module cluster
  N: Bad file data" on `spp.asts.generic_parameter_ast`, in `substitution.cpp` only, deterministic (survives deleting
  the gcm/objects). Triggered by `TypeSymbol::Convention` changing from `Unique<ConventionAst>` to a tag while that TU
  imported both the AST modules and held its memo `Map`s. Fix that worked: move the AST-needing functions into a second
  implementation unit of the same module (`substitution_bindings.cpp`); reordering imports and deleting gcms did not.
  Also: a static member returning `Unique<ConventionAst>` added to `convention_ast.ixx` (with `import spp.utils.types`)
  broke importers the same way - keep such helpers in a module that already imports both (`symbols`).
