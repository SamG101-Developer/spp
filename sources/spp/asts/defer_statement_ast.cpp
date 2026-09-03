module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.defer_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.expr_utils;
import spp.analyse.utils.mem_info_utils;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;

SPP_MOD_BEGIN
spp::asts::DeferStatementAst::DeferStatementAst(
  decltype(TokDefer) &&tok_defer,
  decltype(Expr) &&expr) :
  TokDefer(std::move(tok_defer)),
  Expr(std::move(expr)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokDefer, lex::SppTokenType::KW_DEFER, "defer", Expr ? Expr->PosStart() : 0);
}

spp::asts::DeferStatementAst::~DeferStatementAst() = default;

auto spp::asts::DeferStatementAst::PosStart() const
  -> std::size_t {
  // Use the "defer" token.
  return TokDefer->PosStart();
}

auto spp::asts::DeferStatementAst::PosEnd() const
  -> std::size_t {
  // Use the expression.
  return Expr->PosEnd();
}

auto spp::asts::DeferStatementAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<DeferStatementAst>(
    AstClone(TokDefer),
    AstClone(Expr));
}

auto spp::asts::DeferStatementAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokDefer).append(" ");
  SPP_STRING_APPEND(Expr);
  SPP_STRING_END;
}

auto spp::asts::DeferStatementAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  using analyse::errors::SppDeferTerminatesError;
  using analyse::utils::expr_utils::ValidateDiscardedValue;

  // Marked for the duration of the expression's own analysis,
  // so that a "?" anywhere inside it - however deeply nested
  // - reports against this "defer" rather than expanding into
  // a "ret" that codegen then has to emit at every exit the
  // deferred expression is replayed at.
  const auto saved_defer_tok = meta->WithinDeferTok;
  meta->WithinDeferTok = TokDefer.get();
  Expr->Stage7_AnalyseSemantics(sm, meta);
  meta->WithinDeferTok = saved_defer_tok;

  // Leaving the scope is what runs a deferred expression, so
  // an expression that itself leaves has nowhere sensible to
  // go: it would be unwinding out of the unwind.
  RaiseIf<SppDeferTerminatesError>(
    Expr->Terminates(), {sm->CurrentScope}, ERR_ARGS(*TokDefer, *Expr));

  // Nothing is in a position to receive the value, so there
  // must not be one. This is the ordinary discarded-value
  // rule, which also reports a "case" against the expressions
  // its branches end on rather than against the "case".
  ValidateDiscardedValue(*Expr, sm->CurrentScope, *sm, meta);
}

auto spp::asts::DeferStatementAst::Stage8_CheckMemory(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  //
  auto saved = Vec<Pair<
    Shared<analyse::scopes::VariableSymbol>,
    analyse::utils::mem_info_utils::MemoryInfoSnapshot>>();

  // The expression has to be walked here, in the place it
  // is written, because the walk is what consumes the scopes
  // it owns. But it does not *run* here, so nothing it names
  // may be consumed here - the value has to stay usable for
  // the rest of the scope, which is the entire point of
  // deferring it. So the walk happens, and the memory state
  // it produced is rolled back.
  for (auto const *scope = sm->CurrentScope; scope != nullptr; scope = scope->Parent) {
    for (auto *sym : scope->AllVarSymbols(true)) {
      saved.EmplaceBack(sym->SharedFromThis<analyse::scopes::VariableSymbol>(), sym->MemInfo->Snapshot());
    }
    if (scope == meta->EnclosingFunctionScope) { break; }
  }

  // Registered where it is reached, so an exit written above
  // this statement does not run it - which is what a "defer"
  // means. Guarded against repeats because a loop body is
  // walked twice, and the scope is the same one both times.
  if (not genex::contains(sm->CurrentScope->Deferred, this)) {
    sm->CurrentScope->Deferred.EmplaceBack(this);
  }

  Expr->Stage8_CheckMemory(sm, meta);

  // Whatever the walk moved is what running the expression
  // at a scope exit will move, so that is what gets recorded.
  // Todo: Only whole moves are carried over. A deferred
  //  expression that partially moves a value - taking one
  //  attribute off it rather than the whole thing - is not
  //  accounted for, and the value will still read as owed.
  Consumed.Clear();
  for (auto const &[sym, snapshot] : saved) {
    const auto was_moved = snapshot.AstMoved != nullptr;
    const auto now_moved = spp::get<0>(sym->MemInfo->AstMoved) != nullptr;
    if (not was_moved and now_moved) { Consumed.EmplaceBack(sym->Name); }
    sym->MemInfo->FillFromSnapshot(snapshot);
  }
}

auto spp::asts::DeferStatementAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *)
  -> void {
  //
  using analyse::errors::SppDeferInCompileTimeFunctionError;

  // Only a body being evaluated at compile time reaches this:
  // a function prototype exhausts its scope at stage 9 rather
  // than descending into it. The comptime evaluator has no
  // notion of a scope exit to run the expression at, so
  // rather than silently skipping it, say so.
  // Todo: Use the generic comptime error?
  Raise<SppDeferInCompileTimeFunctionError>({sm->CurrentScope}, ERR_ARGS(*TokDefer));
}

auto spp::asts::DeferStatementAst::Stage11_CodeGen(
  ScopeManager *const sm,
  CompilerMetaData *,
  codegen::LlvmCtx *)
  -> llvm::Value* {
  // Nothing is emitted here: the expression is generated at
  // each of the scope's exits, by "EmitDeferredScope". What
  // reaching this statement does is register it, so that only
  // the exits below it run it - the same thing stage 8 does
  // with "Scope::Deferred". Guarded against repeats because a
  // loop body is walked twice against the same scope.
  if (not genex::contains(sm->CurrentScope->DeferredReached, this)) {
    sm->CurrentScope->DeferredReached.EmplaceBack(this);
  }
  return nullptr;
}

SPP_MOD_END
