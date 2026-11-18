module;
#include <spp/macros.hpp>

module spp.codegen.llvm_defer;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.defer_statement_ast;
import spp.asts.expression_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_drop;
import spp.utils.ice;
import genex;
import llvm;

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
  // only what control actually registered on its way here. Copied, as generating a deferred expression walks its
  // own scopes, which can add to the map.
  const auto found = ctx->Deferred->InScope.find(&scope);
  if (found == ctx->Deferred->InScope.end()) { return; }
  const auto reached = found->second;
  for (auto i = reached.size(); i > 0uz; --i) {
    reached[i - 1uz]->Expr->Stage11_CodeGen(sm, meta, ctx);
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

auto spp::codegen::EmitSuspendDestroy(
  Vec<Vec<analyse::utils::linear_utils::OwnedValue>> const &owned,
  asts::Ast const &at,
  analyse::scopes::ScopeManager *const sm,
  asts::meta::CompilerMetaData *const meta,
  LlvmCtx *const ctx)
  -> void {
  // Each scope out to the coroutine's runs what it deferred, then drops what it still owns, newest first, before the
  // next one out is left. A value partly moved out loses only what is left of it.
  auto const *scope = sm->CurrentScope;
  for (auto const &scope_owned : owned) {
    if (scope == nullptr) { break; }
    EmitDeferredScope(*scope, sm, meta, ctx);
    for (auto const &[name, moved] : scope_owned) {
      const auto sym = scope->FindVarSymbol(name.get(), true);
      if (sym == nullptr or sym->LlvmInfo->Alloca == nullptr) {
        Ice(at, *sm->CurrentScope, "a value owned at a suspension has no storage to drop");
      }
      EmitDrop(sym->TypeRefIn(*scope), sym->LlvmInfo->Alloca, sm, meta, ctx, moved);
    }
    scope = scope == meta->EnclosingFnScope ? nullptr : scope->Parent;
  }
  ctx->Builder.CreateBr(meta->LlvmGenerator->CleanupBlock);
}
