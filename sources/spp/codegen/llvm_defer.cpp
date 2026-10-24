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
import genex;

namespace {
  /// What "scope" deferred, as control leaves it.
  auto EmitDeferredScope(
    spp::analyse::scopes::Scope const &scope,
    spp::analyse::scopes::ScopeManager *const sm,
    spp::asts::meta::CompilerMetaData *const meta,
    spp::codegen::LlvmCtx *const ctx)
    -> void {
    // A path that already branched or returned has nothing left to run: whatever it left through emitted its own
    // deferred expressions there, and anything added here would be unreachable.
    const auto block = ctx->Builder.GetInsertBlock();
    if (block == nullptr or block->hasTerminator()) { return; }

    // Reverse order: the statements run last-registered-first, so a value deferred after another is released first.
    // Read from the reached list rather than from every "defer" the scope contains, so an exit part-way through runs
    // only what control actually registered on its way here. Copied, as generating a deferred expression walks its
    // own scopes, which can add to the map.
    const auto found = ctx->Deferred->InScope.find(&scope);
    if (found == ctx->Deferred->InScope.end()) { return; }
    const auto reached = found->second;
    for (auto i = reached.size(); i > 0uz; --i) {
      reached[i - 1uz]->Expr->Stage11_CodeGen(sm, meta, ctx);
    }
  }
}

auto spp::codegen::EmitScopeExit(
  analyse::scopes::Scope const *const into,
  analyse::scopes::ScopeManager *const sm,
  asts::meta::CompilerMetaData *const meta,
  LlvmCtx *const ctx)
  -> void {
  // An early exit skips the ends of the scopes it jumps out of, so what they deferred runs at the jump instead -
  // innermost first, the order they would have been left in had control reached their ends.
  for (auto const *scope = sm->CurrentScope; scope != nullptr and scope != into; scope = scope->Parent) {
    EmitDeferredScope(*scope, sm, meta, ctx);
  }
}

auto spp::codegen::EnterDeferScope(
  analyse::scopes::Scope const &scope,
  LlvmCtx *const ctx)
  -> void {
  ctx->Deferred->InScope.erase(&scope);
}

auto spp::codegen::ReachDefer(
  analyse::scopes::Scope const &scope,
  asts::DeferStatementAst &defer,
  LlvmCtx *const ctx)
  -> void {
  auto &reached = ctx->Deferred->InScope[&scope];
  if (not genex::contains(reached, &defer)) { reached.emplace_back(&defer); }
}
