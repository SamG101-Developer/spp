module;
#include <spp/macros.hpp>

module spp.codegen.llvm_defer;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.asts.ast;
import spp.asts.defer_statement_ast;
import spp.asts.expression_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.meta.compiler_meta_data;

auto spp::codegen::EmitDeferredScope(
  analyse::scopes::Scope const &scope,
  analyse::scopes::ScopeManager *const sm,
  asts::meta::CompilerMetaData *const meta,
  LlvmCtx *const ctx)
  -> void {
  // A path that already branched or returned has nothing left to run: whatever it left through emitted its own
  // deferred expressions there, and anything added here would be unreachable.
  const auto block = ctx->Builder.GetInsertBlock();
  if (block == nullptr or block->hasTerminator()) { return; }

  // Reverse order: the statements run last-registered-first, so a value deferred after another is released first.
  // Read from the reached list rather than from every "defer" the scope contains, so an exit part-way through runs
  // only what control actually registered on its way here.
  for (auto i = scope.DeferredReached.Len(); i > 0uz; --i) {
    scope.DeferredReached[i - 1uz]->Expr->Stage11_CodeGen(sm, meta, ctx);
  }
}

auto spp::codegen::EmitDeferredUnwind(
  analyse::scopes::Scope const &innermost,
  analyse::scopes::Scope const *const boundary,
  const bool boundary_inclusive,
  analyse::scopes::ScopeManager *const sm,
  asts::meta::CompilerMetaData *const meta,
  LlvmCtx *const ctx)
  -> void {
  // Leaving early skips the scope ends that would otherwise have run the deferred expressions, so every scope between
  // here and the one being jumped out of runs them at the jump instead - innermost first, the order they would have
  // been left in had control reached their ends.
  for (auto const *scope = &innermost; scope != nullptr; scope = scope->Parent) {
    if (scope == boundary and not boundary_inclusive) { break; }
    EmitDeferredScope(*scope, sm, meta, ctx);
    if (scope == boundary) { break; }
  }
}
